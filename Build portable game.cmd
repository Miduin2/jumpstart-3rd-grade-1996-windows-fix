@echo off
setlocal
cd /d "%~dp0"

echo Trampolin 5 / JumpStart 3rd Grade portable builder
echo.
echo First mount the original CD image in File Explorer.
echo Enter the mounted CD drive or the root of a complete CD extraction.
echo Example: F:\
echo Do not enter the folder that merely contains the ISO file.
echo.
set /p "SOURCE_ROOT=CD root: "
if not defined SOURCE_ROOT (
    echo No CD root was entered.
    pause
    exit /b 1
)
set "SOURCE_ROOT=%SOURCE_ROOT:"=%"

echo.
echo Press Enter to use: %USERPROFILE%\Games\JumpStart3
set /p "OUTPUT_ROOT=New output folder: "
if not defined OUTPUT_ROOT set "OUTPUT_ROOT=%USERPROFILE%\Games\JumpStart3"
set "OUTPUT_ROOT=%OUTPUT_ROOT:"=%"

echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-portable-from-cd.ps1" -SourceRoot "%SOURCE_ROOT%\." -OutputRoot "%OUTPUT_ROOT%\."
set "BUILD_RESULT=%ERRORLEVEL%"
echo.
if not "%BUILD_RESULT%"=="0" (
    echo Construction failed. The output above explains what must be corrected.
) else (
    echo Construction completed successfully.
)
pause
exit /b %BUILD_RESULT%
