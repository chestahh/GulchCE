@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
if not exist "halo-workshop-diagnostics.exe" (
    echo Put this launcher beside halo-workshop-diagnostics.exe in your existing GulchCE folder.
    if /I not "%~1"=="--no-pause" pause
    exit /b 2
)
if not exist "SDL3.dll" (
    echo SDL3.dll is missing. Extract the diagnostic files into your existing GulchCE folder.
    if /I not "%~1"=="--no-pause" pause
    exit /b 2
)
set "GULCH_SOURCE_CONFIG=%HALO_CONFIG_FILE%"
if not defined GULCH_SOURCE_CONFIG set "GULCH_SOURCE_CONFIG=%~dp0config.toml"
if not exist "%GULCH_SOURCE_CONFIG%" (
    echo Cannot find the configuration: "%GULCH_SOURCE_CONFIG%"
    if /I not "%~1"=="--no-pause" pause
    exit /b 2
)
:choose_report_folder
set "GULCH_DIAGNOSTIC_DIR=%TEMP%\GulchCE-Workshop-Diagnostics-%RANDOM%"
if exist "%GULCH_DIAGNOSTIC_DIR%" goto choose_report_folder
mkdir "%GULCH_DIAGNOSTIC_DIR%"
if errorlevel 1 exit /b 2
copy /Y "%GULCH_SOURCE_CONFIG%" "%GULCH_DIAGNOSTIC_DIR%\config.toml" >nul
if errorlevel 1 exit /b 2
(
    echo GulchCE Workshop discovery report
    echo Source configuration: "%GULCH_SOURCE_CONFIG%"
    echo Report folder: "%GULCH_DIAGNOSTIC_DIR%"
    echo Discovery environment overrides before launch:
    set HALO_STEAM_ROOT
    set HALO_WORKSHOP_MAPS
    set HALO_MCC_CAMPAIGNS
    set HALO_MCC_COLLECTIONS
    set HALO_CUSTOM_EDITION
    echo Relevant lines in the copied configuration:
    findstr /R /C:"^[ ]*steam[ ]*=" /C:"^[ ]*workshop_maps[ ]*=" /C:"^[ ]*mcc_campaigns[ ]*=" /C:"^[ ]*mcc_collections[ ]*=" /C:"^[ ]*custom_edition[ ]*=" "%GULCH_DIAGNOSTIC_DIR%\config.toml"
) > "%GULCH_DIAGNOSTIC_DIR%\workshop-diagnostics.log" 2>&1
set "HALO_CONFIG_FILE=%GULCH_DIAGNOSTIC_DIR%\config.toml"
set "HALO_SAVE_ROOT=%GULCH_DIAGNOSTIC_DIR%\saves"
set "HALO_LOG_ROOT=%GULCH_DIAGNOSTIC_DIR%"
set "HALO_WORKSHOP_DIAGNOSTICS=1"
set "HALO_UPDATE_ANSWER=never"
set "HALO_CRASH_REPORTS=no"
echo Open the affected MCC map list, reproduce the empty list, then exit the game.
echo Your original config and saves are not used for writing.
"%~dp0halo-workshop-diagnostics.exe" >> "%GULCH_DIAGNOSTIC_DIR%\workshop-diagnostics.log" 2>&1
set "GULCH_DIAGNOSTIC_EXIT=%ERRORLEVEL%"
echo Exit code: %GULCH_DIAGNOSTIC_EXIT% >> "%GULCH_DIAGNOSTIC_DIR%\workshop-diagnostics.log"
echo.
echo Report folder: "%GULCH_DIAGNOSTIC_DIR%"
echo Send workshop-diagnostics.log and debug.txt from that folder.
echo Do not send config.toml or the saves folder.
if /I not "%~1"=="--no-pause" pause
exit /b %GULCH_DIAGNOSTIC_EXIT%
