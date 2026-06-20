@echo off
setlocal EnableDelayedExpansion

set "PROJECT_DIR=%~dp0"
set "EXE=%PROJECT_DIR%Build\Production\Windows\RuneSim.exe"

if not exist "%EXE%" (
    echo ERROR: Build not found at:
    echo   %EXE%
    echo.
    echo Run build.bat first.
    exit /b 1
)

REM --- Scene selection ---------------------------------------------------
REM   run.bat                    -> MainLevel (Cesium)
REM   run.bat <SceneName>        -> any cooked scene by name
REM Any other arguments are passed straight through to the executable.
set "SCENE_ARG="
set "SCENE_NAME="
set "PASSTHRU="
for %%A in (%*) do (
    if /i "%%A"=="clean" (
        set "PASSTHRU=!PASSTHRU! %%A"
    ) else if /i "%%A"=="debug" (
        set "PASSTHRU=!PASSTHRU! %%A"
    ) else if not defined SCENE_NAME (
        set "SCENE_NAME=%%A"
        set "SCENE_ARG=-scene=%%A"
    ) else (
        set "PASSTHRU=!PASSTHRU! %%A"
    )
)

echo Starting RUNE Sim with logging...
if defined SCENE_NAME (echo Scene: !SCENE_NAME!) else (echo Scene: MainLevel ^(Cesium^))
echo Log file: %%LOCALAPPDATA%%\RuneSim\Saved\Logs\RuneSim.log
echo.

start "" "%EXE%" -log -LogCmds="global Warning, LogTemp Warning" !SCENE_ARG!!PASSTHRU!
