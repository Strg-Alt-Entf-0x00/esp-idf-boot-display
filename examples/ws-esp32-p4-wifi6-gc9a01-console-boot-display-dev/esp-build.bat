@echo off
setlocal EnableExtensions

REM ESP32-P4 LP-Core Boot Display - Build and Flash Script
REM Usage: esp-build.bat [build|flash|monitor|clean|menuconfig|all]

cd /d "%~dp0"

set "PROJECT_NAME=boot_display_console_dev"
set "COM_PORT=COM4"
set "LOG_DIR=.logs"
set "LOG_FILE=%LOG_DIR%\esp-build.log"

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

set "IDF_EXPORT="
if defined IDF_PATH if exist "%IDF_PATH%\export.bat" set "IDF_EXPORT=%IDF_PATH%\export.bat"
if not defined IDF_EXPORT if defined IDF_TOOLS_PATH if exist "%IDF_TOOLS_PATH%\v6.0.2\esp-idf\export.bat" set "IDF_EXPORT=%IDF_TOOLS_PATH%\v6.0.2\esp-idf\export.bat"
if not defined IDF_EXPORT if exist "C:\esp\v6.0.2\esp-idf\export.bat" set "IDF_EXPORT=C:\esp\v6.0.2\esp-idf\export.bat"

if not defined IDF_EXPORT (
    echo [ERROR] ESP-IDF export.bat was not found.
    echo Set IDF_PATH or install ESP-IDF 6.0.2 under C:\esp\v6.0.2\esp-idf.
    exit /b 1
)

call "%IDF_EXPORT%"
if errorlevel 1 (
    echo [ERROR] Could not activate ESP-IDF.
    exit /b 1
)

set "ACTION=%~1"
if not defined ACTION set "ACTION=build"

echo.
echo ======================================================================
echo  ESP32-P4 LP-Core Boot Display Console - Build System
echo ======================================================================
echo  Project:  %PROJECT_NAME%
echo  Action:   %ACTION%
echo  COM Port: %COM_PORT%
echo  Log File: %LOG_FILE%
echo ======================================================================
echo.

if /i "%ACTION%"=="build" goto do_build
if /i "%ACTION%"=="flash" goto do_flash
if /i "%ACTION%"=="monitor" goto do_monitor
if /i "%ACTION%"=="clean" goto do_clean
if /i "%ACTION%"=="menuconfig" goto do_menuconfig
if /i "%ACTION%"=="all" goto do_all

echo [ERROR] Unknown action: %ACTION%
echo Usage: esp-build.bat [build^|flash^|monitor^|clean^|menuconfig^|all]
exit /b 1

:do_build
echo [BUILD] Building project...
call idf.py build > "%LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"
type "%LOG_FILE%"
if not "%RESULT%"=="0" (
    echo [ERROR] Build failed. Check log: %LOG_FILE%
    exit /b %RESULT%
)
echo [SUCCESS] Build completed successfully
exit /b 0

:do_flash
echo [FLASH] Flashing to %COM_PORT%...
call idf.py -p %COM_PORT% flash > "%LOG_FILE%" 2>&1
set "RESULT=%ERRORLEVEL%"
type "%LOG_FILE%"
if not "%RESULT%"=="0" (
    echo [ERROR] Flash failed. Check log: %LOG_FILE%
    exit /b %RESULT%
)
echo [SUCCESS] Flash completed successfully
exit /b 0

:do_monitor
echo [MONITOR] Starting serial monitor on %COM_PORT%...
echo [INFO] Press Ctrl+] to exit monitor
call idf.py -p %COM_PORT% monitor
exit /b %ERRORLEVEL%

:do_clean
echo [CLEAN] Cleaning build directory...
call idf.py fullclean
exit /b %ERRORLEVEL%

:do_menuconfig
echo [CONFIG] Opening menuconfig...
call idf.py menuconfig
exit /b %ERRORLEVEL%

:do_all
call :do_build
if errorlevel 1 exit /b 1
call :do_flash
if errorlevel 1 exit /b 1
echo [SUCCESS] Build and flash completed successfully
exit /b 0
