@echo off
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0"
chcp 65001 >nul
setlocal enabledelayedexpansion
color 0F

REM ================================================
REM Initialize environment variables
REM ================================================

REM Get script directory path (scripts folder)
set "SCRIPT_DIR=%~dp0scripts"
REM English is the only supported installer language.
set "LANG_FILE=%SCRIPT_DIR%\languages\en-US.lang"

REM ================================================
REM Message catalogue initialization
REM ================================================

call :LoadEnglishMessages
if errorlevel 1 exit /b 1

echo.
echo "================================================"
echo "           %TITLE%"
echo "================================================"
echo.

REM Check if running with administrator privileges
net session >nul 2>&1
if !errorLevel! neq 0 (
    call :LogError "%ERROR_ADMIN%"
    echo %ERROR_ADMIN_DESC%
    echo.
    echo %ADMIN_ELEVATE_PROMPT%
    set "elevate_choice=Y"
    set /p elevate_choice="%ADMIN_ELEVATE_CONFIRM% "
    if /i "!elevate_choice!"=="Y" (
        echo %ADMIN_ELEVATING%...
        REM Use PowerShell to elevate and restart script
        powershell -Command "Start-Process '%~f0' -Verb RunAs"
        pause
        exit /b 0
    ) else (
        echo %ADMIN_ELEVATE_CANCEL%
        echo.
        pause
        exit /b 1
    )
)

call :LogInfo "%INFO_ADMIN%"
echo.

REM ================================================
REM Environment settings
REM ================================================

REM Set Sunshine root directory path (current directory)
set "SUNSHINE_ROOT=%~dp0"

REM ================================================
REM User confirmation
REM ================================================

REM Display list of operations to be performed and wait for user confirmation
call :ShowUninstallConfirmation
if "%CONFIRM_RESULT%" neq "1" (
    echo %UNINSTALL_CANCEL%
    echo.
    pause
    exit /b 0
)

echo %UNINSTALL_PROCEEDING%
echo.

REM ================================================
REM Uninstall process
REM ================================================

echo "================================================"
echo "           %TITLE_UNINSTALL%"
echo "================================================"
echo.

REM Step 1 - Uninstall virtual display driver (VDD)
call :LogStep "1/5" "%STEP_UNINSTALL_VDD%..."
set "vdd_choice=Y"
set /p vdd_choice="%VDD_UNINSTALL_PROMPT% "
if /i "!vdd_choice!"=="Y" (
    call :LogInfo "%INFO_VDD_UNINSTALL%..."
    REM Call virtual display uninstall script
    call "%SCRIPT_DIR%\uninstall-vdd.bat"
) else (
    call :LogInfo "%INFO_VDD_SKIP%"
)
echo.

REM Step 2 - Uninstall virtual microphone (VB-Cable)
call :LogStep "2/5" "%STEP_UNINSTALL_VSINK%..."
set "vsink_choice=Y"
set /p vsink_choice="%VSINK_UNINSTALL_PROMPT% "
if /i "!vsink_choice!"=="Y" (
    call :LogInfo "%INFO_VSINK_UNINSTALL%..."
    REM Call virtual microphone uninstall script
    call "%SCRIPT_DIR%\uninstall-vsink.bat"
) else (
    call :LogInfo "%INFO_VSINK_SKIP%"
)
echo.

REM Step 3 - Delete firewall rules
call :LogStep "3/5" "%STEP_UNINSTALL_FIREWALL%..."
set "firewall_choice=Y"
set /p firewall_choice="%FIREWALL_UNINSTALL_PROMPT% "
if /i "!firewall_choice!"=="Y" (
    call :LogInfo "%INFO_FIREWALL_UNINSTALL%..."
    call "%SCRIPT_DIR%\delete-firewall-rule.bat"
) else (
    call :LogInfo "%INFO_FIREWALL_SKIP%"
)
echo.

REM Step 4 - Uninstall gamepad support
call :LogStep "4/5" "%STEP_UNINSTALL_GAMEPAD%..."
set "gamepad_choice=Y"
set /p gamepad_choice="%GAMEPAD_UNINSTALL_PROMPT% "
if /i "!gamepad_choice!"=="Y" (
    call :LogInfo "%INFO_GAMEPAD_UNINSTALL%..."
    REM Call gamepad uninstallation script
    call "%SCRIPT_DIR%\uninstall-gamepad.bat"
    if !errorLevel! equ 0 (
        call :LogSuccess "%SUCCESS_GAMEPAD_UNINSTALL%"
    ) else (
        call :LogWarning "%WARNING_GAMEPAD_UNINSTALL%"
    )
) else (
    call :LogInfo "%INFO_GAMEPAD_SKIP%"
)
echo.

REM Step 5 - Delete autostart
call :LogStep "5/5" "%STEP_UNINSTALL_AUTOSTART%..."
set "autostart_choice=Y"
set /p autostart_choice="%AUTOSTART_UNINSTALL_PROMPT% "
if /i "!autostart_choice!"=="Y" (
    call :LogInfo "%INFO_AUTOSTART_UNINSTALL%..."
    set "SHORTCUT_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Sunshine_portable.lnk"
    if exist "!SHORTCUT_PATH!" (
        del "!SHORTCUT_PATH!"
        if !errorLevel! equ 0 (
            call :LogSuccess "%SUCCESS_AUTOSTART_UNINSTALL%"
        ) else (
            call :LogWarning "%WARNING_AUTOSTART_UNINSTALL%"
        )
    ) else (
        call :LogInfo "%INFO_AUTOSTART_NOT_FOUND%"
    )
) else (
    call :LogInfo "%INFO_AUTOSTART_SKIP%"
)
echo.

REM ================================================
REM Completion message
REM ================================================

echo ================================================
echo           %TITLE_SUCCESS%
echo ================================================
echo.
call :LogSuccess "%SUCCESS_MAIN%"
echo.
echo %NOTE_UNINSTALL%
echo.
echo %SCRIPT_COMPLETE_PROMPT%
pause
exit /b 0

REM ================================================
REM Function definition area
REM ================================================

:LogInfo
REM Function - Output info level log (white)
cls
echo [Info] %~1
goto :eof

:LogSuccess
REM Function - Output success level log (green)
cls
powershell -c "Write-Host '[Success] %~1' -ForegroundColor Green"
goto :eof

:LogWarning
REM Function - Output warning level log (yellow)
cls
powershell -c "Write-Host '[Warning] %~1' -ForegroundColor Yellow"
goto :eof

:LogError
REM Function - Output error level log (red)
cls
powershell -c "Write-Host '[Error] %~1' -ForegroundColor Red"
goto :eof

:LogStep
REM Function - Output step level log (cyan)
REM Parameters - %%1 - Step number, %%2 - Step description
REM Returns - None
cls
powershell -c "Write-Host '[%~1] %~2' -ForegroundColor Cyan"
goto :eof


:LoadEnglishMessages
REM Load the fixed English message catalogue.
cls

if not exist "%LANG_FILE%" (
    echo [ERROR] Language file not found: %LANG_FILE%
    pause
    exit /b 1
)

REM Parse the message file and set variables.
for /f "usebackq tokens=1* delims==" %%a in ("%LANG_FILE%") do (
    if not "%%a"=="" if not "%%a:~0,1%"=="#" if not "%%a:~0,2%"=="::" (
        if not "%%b"=="" (
            set "%%a=%%b"
        )
    )
) 2>nul
exit /b 0

:ShowUninstallConfirmation
REM Function - Display uninstall confirmation interface
REM Parameters - None
REM Returns - Set CONFIRM_RESULT variable (1=confirm, 0=cancel)
cls

echo ================================================
echo           %UNINSTALL_CONFIRM_TITLE%
echo ================================================
echo.
echo %UNINSTALL_CONFIRM_DESC%:
echo.
echo 1. %UNINSTALL_CONFIRM_VDD%
echo 2. %UNINSTALL_CONFIRM_VSINK%
echo 3. %UNINSTALL_CONFIRM_FIREWALL%
echo 4. %UNINSTALL_CONFIRM_GAMEPAD%
echo 5. %UNINSTALL_CONFIRM_AUTOSTART%
echo.
set "confirm_choice=Y"
set /p confirm_choice="%UNINSTALL_CONFIRM_PROCEED% "
if /i "!confirm_choice!"=="Y" (
    set "CONFIRM_RESULT=1"
) else (
    set "CONFIRM_RESULT=0"
)
goto :eof
