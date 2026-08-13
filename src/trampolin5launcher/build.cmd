@echo off
setlocal
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GV_VS=%%I"
if not exist "%GV_VS%\Common7\Tools\VsDevCmd.bat" exit /b 1
call "%GV_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /EHsc /DUNICODE /D_UNICODE src\main.cpp user32.lib /link /Brepro /SUBSYSTEM:WINDOWS /OUT:build\PlayTrampolin5.exe
exit /b %errorlevel%
