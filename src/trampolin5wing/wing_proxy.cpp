#include <windows.h>
#include <mmsystem.h>
#include <intrin.h>
#include <winnt.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef TRAMPOLIN_DIAGNOSTICS
#define TRAMPOLIN_DIAGNOSTICS 0
#endif

namespace
{
HMODULE g_instance = nullptr;
HMODULE g_legacy = nullptr;
INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;

using ExitProcessFn = void (WINAPI*)(UINT);
using CreateFileAFn = HANDLE (WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using LoadLibraryAFn = HMODULE (WINAPI*)(LPCSTR);
using PostQuitMessageFn = void (WINAPI*)(int);
using RegisterClassAFn = ATOM (WINAPI*)(const WNDCLASSA*);
using CreateWindowExAFn = HWND (WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
using ShowWindowFn = BOOL (WINAPI*)(HWND, int);
using DestroyWindowFn = BOOL (WINAPI*)(HWND);
using MessageBoxAFn = int (WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
using GetClientRectFn = BOOL (WINAPI*)(HWND, LPRECT);
using WaveOutCloseFn = MMRESULT (WINAPI*)(HWAVEOUT);
using WaveOutResetFn = MMRESULT (WINAPI*)(HWAVEOUT);

ExitProcessFn g_realExitProcess = nullptr;
CreateFileAFn g_realCreateFileA = nullptr;
LoadLibraryAFn g_realLoadLibraryA = nullptr;
PostQuitMessageFn g_realPostQuitMessage = nullptr;
RegisterClassAFn g_realRegisterClassA = nullptr;
CreateWindowExAFn g_realCreateWindowExA = nullptr;
ShowWindowFn g_realShowWindow = nullptr;
DestroyWindowFn g_realDestroyWindow = nullptr;
MessageBoxAFn g_realMessageBoxA = nullptr;
GetClientRectFn g_realGetClientRect = nullptr;
WaveOutCloseFn g_realWaveOutClose = nullptr;
WaveOutResetFn g_realWaveOutReset = nullptr;
volatile LONG g_soundClosePatched = 0;

HWND g_renderWindow = nullptr;
HWND g_rootWindow = nullptr;
WNDPROC g_originalWindowProc = nullptr;
bool g_presentationReady = false;

constexpr int kGameWidth = 640;
constexpr int kGameHeight = 480;
constexpr bool kEnableDiagnosticLogging = TRAMPOLIN_DIAGNOSTICS != 0;
constexpr bool kTraceRendering = false;

void Log(const char* format, ...)
{
    if (!kEnableDiagnosticLogging)
    {
        return;
    }
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_instance, path, MAX_PATH))
    {
        return;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
    {
        return;
    }
    slash[1] = L'\0';
    wcscat_s(path, L"WinG-proxy.log");

    char line[1024] = {};
    va_list args;
    va_start(args, format);
    const int length = vsprintf_s(line, format, args);
    va_end(args);
    if (length <= 0)
    {
        return;
    }

    HANDLE file = CreateFileW(
        path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(file);
}

BOOL CALLBACK LoadLegacy(PINIT_ONCE, PVOID, PVOID*)
{
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_instance, path, MAX_PATH))
    {
        return FALSE;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
    {
        return FALSE;
    }
    slash[1] = L'\0';
    wcscat_s(path, L"WING32.legacy.dll");
    g_legacy = LoadLibraryW(path);
    Log("LOAD legacy=%p error=%lu\r\n", g_legacy, GetLastError());
    return g_legacy != nullptr;
}

FARPROC Resolve(const char* name)
{
    if (!InitOnceExecuteOnce(&g_initOnce, LoadLegacy, nullptr, nullptr))
    {
        Log("RESOLVE %s: legacy load failed error=%lu\r\n", name, GetLastError());
        return nullptr;
    }
    FARPROC proc = GetProcAddress(g_legacy, name);
    Log("RESOLVE %s=%p error=%lu\r\n", name, proc, GetLastError());
    return proc;
}

template<typename T>
T Function(const char* name)
{
    static T function = reinterpret_cast<T>(Resolve(name));
    return function;
}

std::uintptr_t ModuleOffset(void* address)
{
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    auto* caller = reinterpret_cast<std::uint8_t*>(address);
    return caller >= base ? static_cast<std::uintptr_t>(caller - base) : 0;
}

bool PatchImport(
    HMODULE module, const char* importedModule, const char* importedFunction,
    void* replacement, void** original)
{
    auto* base = reinterpret_cast<std::uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (; directory.VirtualAddress && descriptor->Name; ++descriptor)
    {
        const char* moduleName = reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(moduleName, importedModule) != 0)
        {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; names && names->u1.AddressOfData; ++names, ++addresses)
        {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
            {
                continue;
            }
            auto* import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), importedFunction) != 0)
            {
                continue;
            }
            DWORD oldProtection = 0;
            if (!VirtualProtect(&addresses->u1.Function, sizeof(addresses->u1.Function), PAGE_READWRITE, &oldProtection))
            {
                return false;
            }
            if (original)
            {
                *original = reinterpret_cast<void*>(addresses->u1.Function);
            }
            addresses->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            DWORD ignored = 0;
            VirtualProtect(&addresses->u1.Function, sizeof(addresses->u1.Function), oldProtection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), &addresses->u1.Function, sizeof(addresses->u1.Function));
            return true;
        }
    }
    return false;
}

struct WaveCloseContext
{
    HWAVEOUT output;
    MMRESULT result;
};

DWORD WINAPI CloseWaveDevice(LPVOID parameter)
{
    auto* context = static_cast<WaveCloseContext*>(parameter);
    context->result = g_realWaveOutClose
        ? g_realWaveOutClose(context->output)
        : MMSYSERR_ERROR;
    Log("waveOutClose worker device=%p result=%u\r\n",
        context->output, context->result);
    return 0;
}

MMRESULT WINAPI HookWaveOutClose(HWAVEOUT output)
{
    const MMRESULT resetResult = g_realWaveOutReset
        ? g_realWaveOutReset(output)
        : MMSYSERR_ERROR;
    Log("waveOutClose reset device=%p result=%u\r\n", output, resetResult);

    auto* context = static_cast<WaveCloseContext*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WaveCloseContext)));
    if (!context)
    {
        return resetResult == MMSYSERR_NOERROR ? MMSYSERR_NOERROR : MMSYSERR_NOMEM;
    }
    context->output = output;
    context->result = MMSYSERR_ERROR;
    HANDLE worker = CreateThread(nullptr, 0, CloseWaveDevice, context, 0, nullptr);
    if (!worker)
    {
        HeapFree(GetProcessHeap(), 0, context);
        return resetResult == MMSYSERR_NOERROR ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
    }

    constexpr DWORD kCloseTimeoutMilliseconds = 2000;
    const DWORD waitResult = WaitForSingleObject(worker, kCloseTimeoutMilliseconds);
    if (waitResult == WAIT_OBJECT_0)
    {
        const MMRESULT closeResult = context->result;
        CloseHandle(worker);
        HeapFree(GetProcessHeap(), 0, context);
        Log("waveOutClose completed device=%p result=%u\r\n", output, closeResult);
        return closeResult;
    }

    // Some modern audio drivers never return from this 1998 library's final
    // waveOutClose call. Do not terminate the worker while it may hold driver
    // locks; abandon its tiny context and allow process shutdown to reap it.
    CloseHandle(worker);
    Log("waveOutClose timeout device=%p wait=%lu; allowing shutdown\r\n",
        output, waitResult);
    return resetResult == MMSYSERR_NOERROR ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
}

bool PatchSoundClose(HMODULE soundModule)
{
    if (!soundModule)
    {
        return false;
    }
    if (InterlockedCompareExchange(&g_soundClosePatched, 1, 0) != 0)
    {
        return true;
    }

    HMODULE multimedia = GetModuleHandleW(L"WINMM.dll");
    g_realWaveOutReset = multimedia
        ? reinterpret_cast<WaveOutResetFn>(
            GetProcAddress(multimedia, "waveOutReset"))
        : nullptr;
    void* originalClose = nullptr;
    const bool patched = g_realWaveOutReset && PatchImport(
        soundModule, "WINMM.dll", "waveOutClose",
        reinterpret_cast<void*>(HookWaveOutClose), &originalClose);
    if (patched)
    {
        g_realWaveOutClose = reinterpret_cast<WaveOutCloseFn>(originalClose);
    }
    else
    {
        InterlockedExchange(&g_soundClosePatched, 0);
    }
    Log("PATCH sound=%p waveOutReset=%p waveOutClose=%p result=%d error=%lu\r\n",
        soundModule, g_realWaveOutReset, g_realWaveOutClose,
        patched ? 1 : 0, GetLastError());
    return patched;
}

struct PresentationRect
{
    int x;
    int y;
    int width;
    int height;
};

PresentationRect GetPresentationRect(HWND window)
{
    RECT client = {};
    GetClientRect(window, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;

    if (window != g_rootWindow)
    {
        return {0, 0, clientWidth, clientHeight};
    }

    int contentWidth = clientHeight * kGameWidth / kGameHeight;
    int contentHeight = clientHeight;
    if (contentWidth > clientWidth)
    {
        contentWidth = clientWidth;
        contentHeight = clientWidth * kGameHeight / kGameWidth;
    }
    return {
        (clientWidth - contentWidth) / 2,
        (clientHeight - contentHeight) / 2,
        contentWidth,
        contentHeight
    };
}

bool IsMouseMessage(UINT message)
{
    switch (message)
    {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
        return true;
    default:
        return false;
    }
}

LRESULT CALLBACK PresentationWindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_presentationReady && window == g_renderWindow)
    {
        if (IsMouseMessage(message))
        {
            const PresentationRect presentation = GetPresentationRect(window);
            if (presentation.width > 0 && presentation.height > 0)
            {
                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));
                x = MulDiv(x - presentation.x, kGameWidth, presentation.width);
                y = MulDiv(y - presentation.y, kGameHeight, presentation.height);
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x >= kGameWidth) x = kGameWidth - 1;
                if (y >= kGameHeight) y = kGameHeight - 1;
                lParam = MAKELPARAM(static_cast<short>(x), static_cast<short>(y));
            }
        }
        else if (message == WM_SIZE)
        {
            lParam = MAKELPARAM(kGameWidth, kGameHeight);
        }
    }
    if (message == WM_CLOSE || message == WM_DESTROY ||
        message == WM_NCDESTROY || message == WM_ENDSESSION)
    {
        Log("WINDOW_MESSAGE hwnd=%p message=0x%04X wParam=0x%IX lParam=0x%IX\r\n",
            window, message, static_cast<UINT_PTR>(wParam),
            static_cast<UINT_PTR>(lParam));
    }

    LRESULT result = 0;
    DWORD exceptionCode = 0;
    __try
    {
        result = CallWindowProcA(
            g_originalWindowProc, window, message, wParam, lParam);
    }
    __except (exceptionCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        // The 1999 window procedure can raise an exception while Windows is
        // tearing the window down. Letting it escape from a user callback
        // converts an otherwise successful exit into STATUS_FATAL_USER_CALLBACK_EXCEPTION.
    }

    if (exceptionCode != 0)
    {
        Log("WINDOW_PROC_EXCEPTION hwnd=%p message=0x%04X code=0x%08lX "
            "wParam=0x%IX lParam=0x%IX\r\n",
            window, message, exceptionCode,
            static_cast<UINT_PTR>(wParam), static_cast<UINT_PTR>(lParam));

        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
        }
        else if (message == WM_DESTROY || message == WM_NCDESTROY)
        {
            PostQuitMessage(0);
        }
        return 0;
    }

    if (message == WM_NCDESTROY)
    {
        g_presentationReady = false;
        g_renderWindow = nullptr;
        g_rootWindow = nullptr;
        g_originalWindowProc = nullptr;
    }
    return result;
}

void PreparePresentation(HWND renderWindow)
{
    if (g_presentationReady || !renderWindow)
    {
        return;
    }

    g_renderWindow = renderWindow;
    g_rootWindow = GetAncestor(renderWindow, GA_ROOT);
    if (!g_rootWindow)
    {
        g_rootWindow = renderWindow;
    }

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int contentWidth = screenHeight * kGameWidth / kGameHeight;
    int contentHeight = screenHeight;
    if (contentWidth > screenWidth)
    {
        contentWidth = screenWidth;
        contentHeight = screenWidth * kGameHeight / kGameWidth;
    }
    const int contentX = (screenWidth - contentWidth) / 2;
    const int contentY = (screenHeight - contentHeight) / 2;

    LONG_PTR rootStyle = GetWindowLongPtrA(g_rootWindow, GWL_STYLE);
    rootStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX | WS_SYSMENU);
    rootStyle |= WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN;
    SetWindowLongPtrA(g_rootWindow, GWL_STYLE, rootStyle);

    LONG_PTR rootExStyle = GetWindowLongPtrA(g_rootWindow, GWL_EXSTYLE);
    rootExStyle &= ~WS_EX_TOOLWINDOW;
    rootExStyle |= WS_EX_APPWINDOW;
    SetWindowLongPtrA(g_rootWindow, GWL_EXSTYLE, rootExStyle);

    SetWindowPos(
        g_rootWindow, nullptr, 0, 0, screenWidth, screenHeight,
        SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    if (g_renderWindow != g_rootWindow)
    {
        LONG_PTR renderStyle = GetWindowLongPtrA(g_renderWindow, GWL_STYLE);
        renderStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
            WS_MAXIMIZEBOX | WS_SYSMENU);
        renderStyle |= WS_CHILD | WS_VISIBLE;
        SetWindowLongPtrA(g_renderWindow, GWL_STYLE, renderStyle);
        MoveWindow(
            g_renderWindow, contentX, contentY,
            contentWidth, contentHeight, TRUE);
    }

    g_originalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
        g_renderWindow, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(PresentationWindowProc)));
    g_presentationReady = g_originalWindowProc != nullptr;

    Log("PRESENT root=%p render=%p screen=%dx%d content=%d,%d %dx%d subclass=%d error=%lu\r\n",
        g_rootWindow, g_renderWindow, screenWidth, screenHeight,
        contentX, contentY, contentWidth, contentHeight,
        g_presentationReady ? 1 : 0, GetLastError());
}
}

void WINAPI HookExitProcess(UINT code)
{
    Log("ExitProcess code=%u callerRva=0x%08IX\r\n", code, ModuleOffset(_ReturnAddress()));
    g_realExitProcess(code);
}

HANDLE WINAPI HookCreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD creation, DWORD flags, HANDLE templateFile)
{
    HANDLE result = g_realCreateFileA(name, access, share, security, creation, flags, templateFile);
    const DWORD error = GetLastError();
    Log("CreateFileA name=\"%s\" creation=%lu result=%p error=%lu callerRva=0x%08IX\r\n",
        name ? name : "(null)", creation, result, error, ModuleOffset(_ReturnAddress()));
    SetLastError(error);
    return result;
}

HMODULE WINAPI HookLoadLibraryA(LPCSTR name)
{
    HMODULE result = g_realLoadLibraryA(name);
    const DWORD error = GetLastError();
    Log("LoadLibraryA name=\"%s\" result=%p error=%lu callerRva=0x%08IX\r\n",
        name ? name : "(null)", result, error, ModuleOffset(_ReturnAddress()));
    SetLastError(error);
    return result;
}

void WINAPI HookPostQuitMessage(int code)
{
    Log("PostQuitMessage code=%d callerRva=0x%08IX\r\n", code, ModuleOffset(_ReturnAddress()));
    g_realPostQuitMessage(code);
}

ATOM WINAPI HookRegisterClassA(const WNDCLASSA* windowClass)
{
    ATOM result = g_realRegisterClassA(windowClass);
    Log("RegisterClassA class=\"%s\" result=%u error=%lu callerRva=0x%08IX\r\n",
        windowClass && windowClass->lpszClassName ? windowClass->lpszClassName : "(null)",
        result, GetLastError(), ModuleOffset(_ReturnAddress()));
    return result;
}

HWND WINAPI HookCreateWindowExA(
    DWORD extendedStyle, LPCSTR className, LPCSTR title, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu,
    HINSTANCE instance, LPVOID parameter)
{
    HWND result = g_realCreateWindowExA(
        extendedStyle, className, title, style, x, y, width, height,
        parent, menu, instance, parameter);
    Log("CreateWindowExA class=\"%s\" title=\"%s\" style=0x%08lX ex=0x%08lX xy=%d,%d size=%dx%d parent=%p result=%p error=%lu callerRva=0x%08IX\r\n",
        className ? className : "(null)", title ? title : "(null)",
        style, extendedStyle, x, y, width, height, parent, result,
        GetLastError(), ModuleOffset(_ReturnAddress()));
    return result;
}

BOOL WINAPI HookShowWindow(HWND window, int command)
{
    BOOL result = g_realShowWindow(window, command);
    Log("ShowWindow hwnd=%p command=%d result=%d callerRva=0x%08IX\r\n",
        window, command, result, ModuleOffset(_ReturnAddress()));
    return result;
}

BOOL WINAPI HookDestroyWindow(HWND window)
{
    Log("DestroyWindow hwnd=%p callerRva=0x%08IX\r\n", window, ModuleOffset(_ReturnAddress()));
    return g_realDestroyWindow(window);
}

int WINAPI HookMessageBoxA(HWND window, LPCSTR text, LPCSTR caption, UINT type)
{
    Log("MessageBoxA caption=\"%s\" text=\"%s\" type=0x%08X callerRva=0x%08IX\r\n",
        caption ? caption : "(null)", text ? text : "(null)", type, ModuleOffset(_ReturnAddress()));
    return g_realMessageBoxA(window, text, caption, type);
}

BOOL WINAPI HookGetClientRect(HWND window, LPRECT rect)
{
    const BOOL result = g_realGetClientRect(window, rect);
    if (result && g_presentationReady && window == g_renderWindow && rect)
    {
        rect->left = 0;
        rect->top = 0;
        rect->right = kGameWidth;
        rect->bottom = kGameHeight;
    }
    return result;
}

extern "C" BOOL WINAPI WinGBitBlt(
    HDC destination, int x, int y, int width, int height,
    HDC source, int sourceX, int sourceY)
{
    using Fn = BOOL (WINAPI*)(HDC, int, int, int, int, HDC, int, int);
    Fn fn = Function<Fn>("WinGBitBlt");
    if (InterlockedCompareExchange(&g_soundClosePatched, 0, 0) == 0)
    {
        PatchSoundClose(GetModuleHandleW(L"WSOUND32.DLL"));
    }
    HWND destinationWindow = WindowFromDC(destination);
    if (!g_presentationReady && destinationWindow &&
        x == 0 && y == 0 && width == kGameWidth && height == kGameHeight)
    {
        PreparePresentation(destinationWindow);
    }

    BOOL result = FALSE;
    if (g_presentationReady && destinationWindow == g_renderWindow &&
        width > 0 && height > 0)
    {
        const PresentationRect presentation = GetPresentationRect(g_renderWindow);
        const int left = presentation.x + MulDiv(x, presentation.width, kGameWidth);
        const int top = presentation.y + MulDiv(y, presentation.height, kGameHeight);
        const int right = presentation.x + MulDiv(x + width, presentation.width, kGameWidth);
        const int bottom = presentation.y + MulDiv(y + height, presentation.height, kGameHeight);

        if (x == 0 && y == 0 && width == kGameWidth && height == kGameHeight &&
            g_renderWindow == g_rootWindow)
        {
            RECT client = {};
            GetClientRect(g_renderWindow, &client);
            HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            if (presentation.x > 0)
            {
                RECT bar = {0, 0, presentation.x, client.bottom};
                FillRect(destination, &bar, black);
                bar.left = presentation.x + presentation.width;
                bar.right = client.right;
                FillRect(destination, &bar, black);
            }
            if (presentation.y > 0)
            {
                RECT bar = {0, 0, client.right, presentation.y};
                FillRect(destination, &bar, black);
                bar.top = presentation.y + presentation.height;
                bar.bottom = client.bottom;
                FillRect(destination, &bar, black);
            }
        }

        const int previousMode = SetStretchBltMode(destination, COLORONCOLOR);
        result = StretchBlt(
            destination, left, top, right - left, bottom - top,
            source, sourceX, sourceY, width, height, SRCCOPY);
        SetStretchBltMode(destination, previousMode);
    }
    else
    {
        result = fn ? fn(destination, x, y, width, height, source, sourceX, sourceY) : FALSE;
    }
    if constexpr (kTraceRendering)
    {
        Log("WinGBitBlt dst=%p src=%p xy=%d,%d size=%dx%d source=%d,%d result=%d error=%lu\r\n",
            destination, source, x, y, width, height, sourceX, sourceY, result, GetLastError());
    }
    return result;
}

extern "C" HBITMAP WINAPI WinGCreateBitmap(
    HDC dc, BITMAPINFO* info, void** bits)
{
    using Fn = HBITMAP (WINAPI*)(HDC, BITMAPINFO*, void**);
    Fn fn = Function<Fn>("WinGCreateBitmap");
    HBITMAP result = fn ? fn(dc, info, bits) : nullptr;
    const BITMAPINFOHEADER* h = info ? &info->bmiHeader : nullptr;
    Log("WinGCreateBitmap dc=%p bmi=%p width=%ld height=%ld bpp=%u compression=%lu bitmap=%p bits=%p error=%lu\r\n",
        dc, info, h ? h->biWidth : 0, h ? h->biHeight : 0,
        h ? h->biBitCount : 0, h ? h->biCompression : 0,
        result, bits ? *bits : nullptr, GetLastError());
    return result;
}

extern "C" HDC WINAPI WinGCreateDC()
{
    using Fn = HDC (WINAPI*)();
    Fn fn = Function<Fn>("WinGCreateDC");
    HDC result = fn ? fn() : nullptr;
    Log("WinGCreateDC result=%p error=%lu\r\n", result, GetLastError());
    return result;
}

extern "C" HBRUSH WINAPI WinGCreateHalftoneBrush(HDC dc, COLORREF color, UINT type)
{
    using Fn = HBRUSH (WINAPI*)(HDC, COLORREF, UINT);
    Fn fn = Function<Fn>("WinGCreateHalftoneBrush");
    return fn ? fn(dc, color, type) : nullptr;
}

extern "C" HPALETTE WINAPI WinGCreateHalftonePalette()
{
    using Fn = HPALETTE (WINAPI*)();
    Fn fn = Function<Fn>("WinGCreateHalftonePalette");
    return fn ? fn() : nullptr;
}

extern "C" UINT WINAPI WinGGetDIBColorTable(HDC dc, UINT start, UINT count, RGBQUAD* colors)
{
    using Fn = UINT (WINAPI*)(HDC, UINT, UINT, RGBQUAD*);
    Fn fn = Function<Fn>("WinGGetDIBColorTable");
    return fn ? fn(dc, start, count, colors) : 0;
}

extern "C" void* WINAPI WinGGetDIBPointer(HBITMAP bitmap, BITMAPINFO* info)
{
    using Fn = void* (WINAPI*)(HBITMAP, BITMAPINFO*);
    Fn fn = Function<Fn>("WinGGetDIBPointer");
    return fn ? fn(bitmap, info) : nullptr;
}

extern "C" BOOL WINAPI WinGRecommendDIBFormat(BITMAPINFO* info)
{
    using Fn = BOOL (WINAPI*)(BITMAPINFO*);
    Fn fn = Function<Fn>("WinGRecommendDIBFormat");
    BOOL result = fn ? fn(info) : FALSE;
    const BITMAPINFOHEADER* h = info ? &info->bmiHeader : nullptr;
    Log("WinGRecommendDIBFormat bmi=%p result=%d width=%ld height=%ld planes=%u bpp=%u compression=%lu colors=%lu error=%lu\r\n",
        info, result, h ? h->biWidth : 0, h ? h->biHeight : 0,
        h ? h->biPlanes : 0, h ? h->biBitCount : 0,
        h ? h->biCompression : 0, h ? h->biClrUsed : 0, GetLastError());
    return result;
}

extern "C" UINT WINAPI WinGSetDIBColorTable(
    HDC dc, UINT start, UINT count, const RGBQUAD* colors)
{
    using Fn = UINT (WINAPI*)(HDC, UINT, UINT, const RGBQUAD*);
    Fn fn = Function<Fn>("WinGSetDIBColorTable");
    UINT result = fn ? fn(dc, start, count, colors) : 0;
    if constexpr (kTraceRendering)
    {
        Log("WinGSetDIBColorTable dc=%p start=%u count=%u result=%u error=%lu\r\n",
            dc, start, count, result, GetLastError());
    }
    return result;
}

extern "C" BOOL WINAPI WinGStretchBlt(
    HDC destination, int x, int y, int width, int height,
    HDC source, int sourceX, int sourceY, int sourceWidth, int sourceHeight)
{
    using Fn = BOOL (WINAPI*)(HDC, int, int, int, int, HDC, int, int, int, int);
    Fn fn = Function<Fn>("WinGStretchBlt");
    return fn ? fn(destination, x, y, width, height, source, sourceX, sourceY, sourceWidth, sourceHeight) : FALSE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
        // The legacy engine releases WinG before it destroys the window whose
        // procedure is subclassed below. Keep this proxy mapped until process
        // termination so that late window messages can never call unloaded
        // code (the historical WING32.DLL_unloaded shutdown crash/hang).
        HMODULE pinnedModule = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(instance), &pinnedModule))
        {
            return FALSE;
        }
        Log("ATTACH pid=%lu\r\n", GetCurrentProcessId());

        HMODULE executable = GetModuleHandleW(nullptr);
        // Production only needs to virtualize the game's 640x480 client area.
        // The other import hooks above are retained as diagnostic tools but
        // deliberately left inactive in normal builds.
        PatchImport(executable, "USER32.dll", "GetClientRect", reinterpret_cast<void*>(HookGetClientRect), reinterpret_cast<void**>(&g_realGetClientRect));
        if constexpr (kEnableDiagnosticLogging)
        {
            PatchImport(executable, "KERNEL32.dll", "ExitProcess", reinterpret_cast<void*>(HookExitProcess), reinterpret_cast<void**>(&g_realExitProcess));
            PatchImport(executable, "USER32.dll", "PostQuitMessage", reinterpret_cast<void*>(HookPostQuitMessage), reinterpret_cast<void**>(&g_realPostQuitMessage));
            PatchImport(executable, "USER32.dll", "DestroyWindow", reinterpret_cast<void*>(HookDestroyWindow), reinterpret_cast<void**>(&g_realDestroyWindow));
            PatchImport(executable, "USER32.dll", "ShowWindow", reinterpret_cast<void*>(HookShowWindow), reinterpret_cast<void**>(&g_realShowWindow));
            PatchImport(executable, "USER32.dll", "CreateWindowExA", reinterpret_cast<void*>(HookCreateWindowExA), reinterpret_cast<void**>(&g_realCreateWindowExA));
            PatchImport(executable, "USER32.dll", "RegisterClassA", reinterpret_cast<void*>(HookRegisterClassA), reinterpret_cast<void**>(&g_realRegisterClassA));
            PatchImport(executable, "USER32.dll", "MessageBoxA", reinterpret_cast<void*>(HookMessageBoxA), reinterpret_cast<void**>(&g_realMessageBoxA));
        }
    }
    return TRUE;
}
