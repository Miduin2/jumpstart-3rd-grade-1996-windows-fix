#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace
{
constexpr wchar_t kMutexName[] = L"Local\\GameVault.Trampolin5.Launcher";
constexpr DWORD kLegacyCleanExit = 0xC000041D;

constexpr int kSystemColorIndices[] = {
    COLOR_SCROLLBAR,
    COLOR_BACKGROUND,
    COLOR_ACTIVECAPTION,
    COLOR_INACTIVECAPTION,
    COLOR_MENU,
    COLOR_WINDOW,
    COLOR_WINDOWFRAME,
    COLOR_MENUTEXT,
    COLOR_WINDOWTEXT,
    COLOR_CAPTIONTEXT,
    COLOR_ACTIVEBORDER,
    COLOR_INACTIVEBORDER,
    COLOR_APPWORKSPACE,
    COLOR_HIGHLIGHT,
    COLOR_HIGHLIGHTTEXT,
    COLOR_BTNFACE,
    COLOR_BTNSHADOW,
    COLOR_GRAYTEXT,
    COLOR_BTNTEXT,
    COLOR_INACTIVECAPTIONTEXT,
    COLOR_BTNHIGHLIGHT,
    COLOR_3DDKSHADOW,
    COLOR_3DLIGHT,
    COLOR_INFOTEXT,
    COLOR_INFOBK,
    COLOR_HOTLIGHT,
    COLOR_GRADIENTACTIVECAPTION,
    COLOR_GRADIENTINACTIVECAPTION,
    COLOR_MENUHILIGHT,
    COLOR_MENUBAR,
};

struct SystemColorSnapshot
{
    COLORREF values[_countof(kSystemColorIndices)] = {};
};

class LegacyDriveAlias
{
public:
    LegacyDriveAlias() = default;
    LegacyDriveAlias(const LegacyDriveAlias&) = delete;
    LegacyDriveAlias& operator=(const LegacyDriveAlias&) = delete;

    ~LegacyDriveAlias()
    {
        Reset();
    }

    bool Create(const std::wstring& target)
    {
        Reset();
        const std::wstring rawTarget = L"\\??\\" + target;
        const DWORD logicalDrives = GetLogicalDrives();
        constexpr wchar_t preferredLetters[] = {L'V', L'W', L'X', L'Y', L'Z'};

        for (const wchar_t letter : preferredLetters)
        {
            const DWORD driveBit = 1u << (letter - L'A');
            if (logicalDrives & driveBit)
            {
                continue;
            }

            const std::wstring deviceName = std::wstring(1, letter) + L":";
            wchar_t existingTarget[4] = {};
            if (QueryDosDeviceW(
                    deviceName.c_str(), existingTarget,
                    static_cast<DWORD>(_countof(existingTarget))) != 0)
            {
                continue;
            }

            if (DefineDosDeviceW(
                    DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM,
                    deviceName.c_str(), rawTarget.c_str()))
            {
                deviceName_ = deviceName;
                rawTarget_ = rawTarget;
                return true;
            }
        }
        return false;
    }

    const std::wstring& DeviceName() const
    {
        return deviceName_;
    }

private:
    void Reset()
    {
        if (!deviceName_.empty())
        {
            DefineDosDeviceW(
                DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE |
                    DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM,
                deviceName_.c_str(), rawTarget_.c_str());
            deviceName_.clear();
            rawTarget_.clear();
        }
    }

    std::wstring deviceName_;
    std::wstring rawTarget_;
};

SystemColorSnapshot CaptureSystemColors()
{
    SystemColorSnapshot snapshot;
    for (std::size_t index = 0; index < _countof(kSystemColorIndices); ++index)
    {
        snapshot.values[index] = GetSysColor(kSystemColorIndices[index]);
    }
    return snapshot;
}

void RestoreSystemColors(const SystemColorSnapshot& snapshot)
{
    int indices[_countof(kSystemColorIndices)] = {};
    COLORREF values[_countof(kSystemColorIndices)] = {};
    for (std::size_t index = 0; index < _countof(kSystemColorIndices); ++index)
    {
        indices[index] = kSystemColorIndices[index];
        values[index] = snapshot.values[index];
    }

    SetSysColors(
        static_cast<int>(_countof(indices)), indices, values);
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(
        HWND_BROADCAST, WM_SYSCOLORCHANGE, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_NORMAL, 2000, &ignored);
    SendMessageTimeoutW(
        HWND_BROADCAST, WM_THEMECHANGED, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_NORMAL, 2000, &ignored);
}

std::wstring JoinPath(const std::wstring& directory, const wchar_t* name)
{
    if (directory.empty() || directory.back() == L'\\')
    {
        return directory + name;
    }
    return directory + L"\\" + name;
}

std::wstring ExecutableDirectory()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
    {
        return {};
    }
    std::wstring result(path.data(), length);
    const std::size_t separator = result.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : result.substr(0, separator);
}

bool FileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirectoryExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool EnsureDirectory(const std::wstring& path)
{
    if (path.empty())
    {
        return false;
    }

    if (DirectoryExists(path))
    {
        return true;
    }

    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator != std::wstring::npos)
    {
        const std::wstring parent = path.substr(0, separator);
        if (!parent.empty() && !DirectoryExists(parent) &&
            !EnsureDirectory(parent))
        {
            return false;
        }
    }

    if (CreateDirectoryW(path.c_str(), nullptr))
    {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS && DirectoryExists(path);
}

bool ReadFileBytes(const std::wstring& path, std::vector<BYTE>& bytes)
{
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    LARGE_INTEGER size = {};
    const bool validSize = GetFileSizeEx(file, &size) &&
        size.QuadPart >= 0 && size.QuadPart <= 16 * 1024 * 1024;
    if (!validSize)
    {
        CloseHandle(file);
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = bytes.empty() || ReadFile(
        file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    return ok && read == bytes.size();
}

bool WriteFileBytes(const std::wstring& path, const void* data, DWORD size)
{
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = size == 0 || WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

std::string NarrowAsciiPath(const std::wstring& value)
{
    const int length = WideCharToMultiByte(
        CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
    {
        return {};
    }
    std::vector<char> buffer(static_cast<std::size_t>(length));
    WideCharToMultiByte(
        CP_ACP, 0, value.c_str(), -1, buffer.data(), length, nullptr, nullptr);
    return std::string(buffer.data());
}

int ShowError(const wchar_t* message)
{
    MessageBoxW(
        nullptr, message, L"Trampolin 5 could not be started",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    const std::wstring editionRoot = ExecutableDirectory();
    const std::wstring splitHardDiskRoot = JoinPath(editionRoot, L"hd");
    const std::wstring splitCompactDiscRoot = JoinPath(editionRoot, L"cd\\3G");
    const bool usesSplitMediaLayout =
        FileExists(JoinPath(splitHardDiskRoot, L"3g.exe")) &&
        FileExists(JoinPath(splitCompactDiscRoot, L"Sound.bal"));
    const std::wstring gameRoot = usesSplitMediaLayout
        ? splitHardDiskRoot
        : JoinPath(editionRoot, L"game");
    const std::wstring compactDiscRoot = usesSplitMediaLayout
        ? splitCompactDiscRoot
        : gameRoot;
    const std::wstring gameExe = JoinPath(gameRoot, L"3g.exe");
    const std::wstring gameIniPath = JoinPath(gameRoot, L"3G.INI");
    const bool isEnglishEdition =
        FileExists(JoinPath(gameRoot, L"SOUND2.BAL")) ||
        FileExists(JoinPath(compactDiscRoot, L"SOUND2.BAL"));
    if (editionRoot.empty() || !FileExists(gameExe) ||
        !FileExists(JoinPath(compactDiscRoot, L"Sound.bal")) ||
        !FileExists(JoinPath(gameRoot, L"WING32.DLL")))
    {
        return ShowError(L"The playable edition is incomplete.");
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex)
    {
        return ShowError(L"The launch lock could not be created.");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return ShowError(L"Trampolin 5 is already running.");
    }

    wchar_t localAppData[32768] = {};
    const DWORD environmentLength = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData, static_cast<DWORD>(_countof(localAppData)));
    if (!environmentLength || environmentLength >= _countof(localAppData))
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"LOCALAPPDATA could not be located.");
    }

    const std::wstring virtualWindows = JoinPath(
        std::wstring(localAppData), L"VirtualStore\\Windows");
    const std::wstring kaIni = JoinPath(virtualWindows, L"KA.INI");
    if (!EnsureDirectory(virtualWindows))
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"The temporary legacy configuration directory could not be created.");
    }

    std::vector<BYTE> previousKaIni;
    const bool hadPreviousKaIni = FileExists(kaIni);
    if (hadPreviousKaIni && !ReadFileBytes(kaIni, previousKaIni))
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"The existing legacy KA.INI could not be backed up safely.");
    }
    std::vector<BYTE> previousGameIni;
    const bool hadPreviousGameIni = FileExists(gameIniPath);
    if (hadPreviousGameIni && !ReadFileBytes(gameIniPath, previousGameIni))
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"The existing 3G.INI could not be backed up safely.");
    }
    const std::string gamePath = NarrowAsciiPath(gameRoot);
    const std::string compactDiscPath = NarrowAsciiPath(compactDiscRoot);
    if (gamePath.empty() || compactDiscPath.empty())
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"The game path cannot be represented for this legacy application.");
    }

    // The legacy multimedia modules store media roots and derived filenames in
    // very small fixed-size buffers. Always expose the package through a short
    // temporary drive. Merged-media packages use its root directly; split
    // Spanish packages retain the installer's HD and CD roots beneath it (for
    // example V:\hd and V:\cd\3G).
    LegacyDriveAlias hardDiskAlias;
    std::string configuredHardDiskPath = gamePath;
    std::string configuredCompactDiscPath = compactDiscPath;
    const std::wstring aliasTarget = usesSplitMediaLayout
        ? editionRoot
        : gameRoot;
    if (!hardDiskAlias.Create(aliasTarget))
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"A temporary short drive alias could not be created.");
    }
    if (usesSplitMediaLayout)
    {
        const std::string packageDrive = NarrowAsciiPath(hardDiskAlias.DeviceName());
        configuredHardDiskPath = packageDrive + "\\hd";
        configuredCompactDiscPath = packageDrive + "\\cd\\3G";
    }
    else
    {
        configuredHardDiskPath = NarrowAsciiPath(hardDiskAlias.DeviceName());
        configuredCompactDiscPath = configuredHardDiskPath;
    }

    const std::string productName = isEnglishEdition
        ? "JumpStart 3rd Grade"
        : "Educacion Primaria 5 Curso";
    const std::string newline = isEnglishEdition ? "\r\n" : "\n";
    const std::string kaContent =
        "[3G]" + newline +
        "ProductHDRoot=" + configuredHardDiskPath + newline +
        "ProductCDRoot=" + configuredCompactDiscPath + newline +
        "ProductCDRoot2=" + configuredCompactDiscPath + newline +
        "HDRoot=" + configuredHardDiskPath + newline +
        "CDRoot=" + configuredCompactDiscPath + newline +
        "CDRoot2=" + configuredCompactDiscPath + newline +
        "ShortTit=3G" + newline +
        "Product=" + productName + newline +
        "TuneFileExtension=WGM" + newline +
        "Memory=Full" + newline;
    std::string gameIni;
    if (isEnglishEdition)
    {
        gameIni =
            "[Paths]\r\n"
            "CDPath=" + configuredCompactDiscPath + "\\\r\n"
            "HDPath=" + configuredHardDiskPath + "\\\r\n"
            "SoundCDPath=" + configuredCompactDiscPath + "\\\r\n\r\n";
    }
    gameIni +=
        "[Video]" + newline +
        "FullScreen=0" + newline +
        "HideTaskBar=0" + newline +
        "NoWarnings=1" + newline + newline +
        "[CONTROL]" + newline +
        "TuneFileExtension=WGM" + newline;

    if (!WriteFileBytes(
            kaIni, kaContent.data(), static_cast<DWORD>(kaContent.size())) ||
        !WriteFileBytes(
            gameIniPath, gameIni.data(),
            static_cast<DWORD>(gameIni.size())))
    {
        if (hadPreviousKaIni)
        {
            WriteFileBytes(
                kaIni, previousKaIni.data(),
                static_cast<DWORD>(previousKaIni.size()));
        }
        else
        {
            DeleteFileW(kaIni.c_str());
        }
        if (hadPreviousGameIni)
        {
            WriteFileBytes(
                gameIniPath, previousGameIni.data(),
                static_cast<DWORD>(previousGameIni.size()));
        }
        else
        {
            DeleteFileW(gameIniPath.c_str());
        }
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return ShowError(L"The temporary legacy configuration could not be written.");
    }

    const SystemColorSnapshot systemColors = CaptureSystemColors();
    std::wstring command = L"\"" + gameExe + L"\"";
    const std::wstring launchWorkingDirectory = usesSplitMediaLayout
        ? hardDiskAlias.DeviceName() + L"\\hd"
        : hardDiskAlias.DeviceName() + L"\\";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessW(
        gameExe.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_DEFAULT_ERROR_MODE, nullptr, launchWorkingDirectory.c_str(),
        &startup, &process);

    DWORD exitCode = ERROR_GEN_FAILURE;
    if (created)
    {
        CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hProcess);
    }

    // This WinG-era application temporarily changes global system colors.
    // Restore them even when the child exits abnormally so Explorer and other
    // modern applications cannot be left using the legacy 256-color palette.
    RestoreSystemColors(systemColors);

    if (hadPreviousKaIni)
    {
        WriteFileBytes(
            kaIni, previousKaIni.data(),
            static_cast<DWORD>(previousKaIni.size()));
    }
    else
    {
        DeleteFileW(kaIni.c_str());
    }
    if (hadPreviousGameIni)
    {
        WriteFileBytes(
            gameIniPath, previousGameIni.data(),
            static_cast<DWORD>(previousGameIni.size()));
    }
    else
    {
        DeleteFileW(gameIniPath.c_str());
    }

    ReleaseMutex(mutex);
    CloseHandle(mutex);

    if (!created)
    {
        return ShowError(L"Windows could not open 3g.exe.");
    }
    // This exact 1999 executable finishes its save and credits sequence before
    // an obsolete multimedia callback raises this otherwise harmless status.
    return exitCode == kLegacyCleanExit ? 0 : static_cast<int>(exitCode);
}
