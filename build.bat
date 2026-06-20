@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT_FILE=%PROJECT_DIR%RuneSim.uproject"
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
set "RUN_UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"
set "ARCHIVE_DIR=%PROJECT_DIR%Build\Production"
set "OUTPUT_DIR=%ARCHIVE_DIR%\Windows"
set "ENGINE_CESIUM_DESCRIPTOR="
for /d %%d in ("%UE_ROOT%\Engine\Plugins\Marketplace\Cesium*") do (
    if exist "%%d\CesiumForUnreal.uplugin" set "ENGINE_CESIUM_DESCRIPTOR=%%d\CesiumForUnreal.uplugin"
)
set "CLEAN_ARG="
set "CLIENT_CONFIG=Shipping"

for %%A in (%*) do (
    if /i "%%A"=="clean"    set "CLEAN_ARG=-clean"
    if /i "%%A"=="-clean"   set "CLEAN_ARG=-clean"
    if /i "%%A"=="--clean"  set "CLEAN_ARG=-clean"
    if /i "%%A"=="debug"    set "CLIENT_CONFIG=Development"
    if /i "%%A"=="-debug"   set "CLIENT_CONFIG=Development"
    if /i "%%A"=="--debug"  set "CLIENT_CONFIG=Development"
)

if not exist "%PROJECT_FILE%" (
    echo ERROR: Could not find project file:
    echo   %PROJECT_FILE%
    exit /b 1
)

if not exist "%RUN_UAT%" (
    echo ERROR: Could not find Unreal Automation Tool:
    echo   %RUN_UAT%
    echo.
    echo Update UE_ROOT in this script if Unreal Engine is installed elsewhere.
    exit /b 1
)

if "%ENGINE_CESIUM_DESCRIPTOR%"=="" (
    echo ERROR: Required Unreal Engine Marketplace plugin is missing:
    echo   Cesium for Unreal
    echo.
    echo Searched under:
    echo   %UE_ROOT%\Engine\Plugins\Marketplace\
    echo.
    echo Install it from the Epic Games Launcher:
    echo   1. Open Epic Games Launcher.
    echo   2. Go to Unreal Engine ^> Library.
    echo   3. Under Vault, find or search for "Cesium for Unreal".
    echo   4. Click Install to Engine.
    echo   5. Choose Unreal Engine 5.7.
    echo   6. Run this build script again.
    echo.
    echo If the plugin is already installed elsewhere, update ENGINE_CESIUM_DESCRIPTOR
    echo in this script to point to its CesiumForUnreal.uplugin file.
    exit /b 1
)

echo Building RUNE Sim production package...
echo Project:
echo   %PROJECT_FILE%
echo Archive directory:
echo   %ARCHIVE_DIR%
echo Final packaged build will be written to:
echo   %OUTPUT_DIR%
echo Build config: %CLIENT_CONFIG%
if defined CLEAN_ARG echo Clean build: enabled
echo.

call "%RUN_UAT%" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 ^
    -target=RuneSim ^
    -platform=Win64 ^
    -clientconfig=%CLIENT_CONFIG% ^
    %CLEAN_ARG% ^
    -build ^
    -cook ^
    -allmaps ^
    -stage ^
    -pak ^
    -compressed ^
    -archive ^
    -archivedirectory="%ARCHIVE_DIR%" ^
    -utf8output

if errorlevel 1 (
    echo.
    echo Production build failed.
    exit /b 1
)

echo.
echo Production build complete.
echo Written to:
echo   %OUTPUT_DIR%
echo.
echo Available scenes:
echo   run.bat                    MainLevel ^(Cesium, default^)
for %%F in ("%PROJECT_DIR%Content\*.umap") do (
    if /i not "%%~nF"=="MainLevel" if /i not "%%~nF"=="scnery" (
        echo   run.bat %%~nF
    )
)
