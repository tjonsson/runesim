@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "EXE=%PROJECT_DIR%Build\Production\Windows\RuneSim.exe"

if not exist "%EXE%" (
    echo ERROR: Build not found at:
    echo   %EXE%
    echo.
    echo Run build.bat first.
    exit /b 1
)

echo Starting RUNE Sim with logging...
echo Log file: %%LOCALAPPDATA%%\RuneSim\Saved\Logs\RuneSim.log
echo.

start "" "%EXE%" -log -LogCmds="global Warning, LogTemp Warning" %*
