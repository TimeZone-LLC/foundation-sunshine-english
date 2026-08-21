@echo off
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0"
setlocal enabledelayedexpansion

rem Turn on boot start for an already-installed service. The installer now folds
rem this into install-service.bat (--start-type), so this script only serves
rem portable installs and the manual recipe in the docs.
set SERVICE_NAME=SunshineService
set "SERVICE_REG_KEY=HKLM\SYSTEM\CurrentControlSet\Services\%SERVICE_NAME%"

sc qc %SERVICE_NAME% >nul 2>&1
if not %ERRORLEVEL%==0 (
    echo ERROR: %SERVICE_NAME% is not installed. Run install-service.bat first.
    exit /b 1
)

rem Never clobber an existing auto-start configuration: install-service.bat may
rem have just restored delayed-auto, and plain `auto` would silently downgrade
rem it. Read the registry rather than `sc qc`, whose column labels are localized.
set "CURRENT_START="
for /f "tokens=3" %%a in ('reg query "%SERVICE_REG_KEY%" /v Start 2^>nul ^| findstr /I /C:"REG_DWORD"') do set "CURRENT_START=%%a"

if /I "!CURRENT_START!"=="0x2" (
    echo %SERVICE_NAME% already starts automatically.
    exit /b 0
)

rem Delayed start keeps boot fast and lets the GPU/driver stack settle before
rem capture starts.
sc config %SERVICE_NAME% start= delayed-auto
if errorlevel 1 (
    echo ERROR: Failed to set auto-start for %SERVICE_NAME%.
    exit /b 1
)

echo %SERVICE_NAME% will now start automatically after a delayed boot.
exit /b 0
