@echo off
setlocal
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GV_VS=%%I"
if not exist "%GV_VS%\Common7\Tools\VsDevCmd.bat" exit /b 1
call "%GV_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
cl /nologo /std:c++17 /LD /O2 /EHsc wing_proxy.cpp user32.lib gdi32.lib /link /Brepro /DEF:exports.def /OUT:WING32.dll
exit /b %errorlevel%
