@echo off
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0"
setlocal enabledelayedexpansion

rem --start-type <auto|delayed-auto|demand|disabled> is an explicit, authoritative
rem choice; the installer only passes it on interactive runs, where the user just
rem confirmed the "Launch on system startup" checkbox. Without it the start type
rem of an already-registered service is preserved, so reinstalls and silent
rem auto-updates can never change how Sunshine starts.
set "SKIP_START="
set "FORCED_START_TYPE="
set "NEXT_IS_START_TYPE="
for %%A in (%*) do (
    if defined NEXT_IS_START_TYPE (
        set "FORCED_START_TYPE=%%~A"
        set "NEXT_IS_START_TYPE="
    ) else if /I "%%~A"=="--no-start" (
        set "SKIP_START=1"
    ) else if /I "%%~A"=="--start-type" (
        set "NEXT_IS_START_TYPE=1"
    )
)

rem Get sunshine root directory
for %%I in ("%~dp0\..") do set "ROOT_DIR=%%~fI"

set SERVICE_NAME=SunshineService
set "SERVICE_BIN=%ROOT_DIR%\tools\sunshinesvc.exe"
set "SERVICE_REG_KEY=HKLM\SYSTEM\CurrentControlSet\Services\%SERVICE_NAME%"
set "KEEP_START_TYPE="
set "SERVICE_CONFIG_DIR=%LOCALAPPDATA%\LizardByte\Sunshine"
set "SERVICE_CONFIG_FILE=%SERVICE_CONFIG_DIR%\service_start_type.txt"

if not exist "%SERVICE_BIN%" (
    echo ERROR: Service binary not found: "%SERVICE_BIN%"
    exit /b 1
)

rem Default for a genuinely new registration. A streaming host is expected to be
rem reachable after a reboot without anyone logging in first, and delayed start
rem keeps boot fast while the GPU/driver stack settles before capture starts.
set SERVICE_START_TYPE=delayed-auto

rem Remove the legacy SunshineSvc service (NSIS-era name). On clean installs
rem this service obviously does not exist, so both commands return error
rem 1060 / "service name invalid". Silence stdout+stderr — we don't care
rem about the failure, and the noise was confusing users into thinking
rem something was wrong with the new install.
net stop sunshinesvc >nul 2>&1
sc delete sunshinesvc >nul 2>&1

rem Decide: reconfigure existing service vs create a new one.
rem `sc config` happily updates binPath in-place, so cross-directory
rem reinstalls don't need a delete+recreate dance.
set "SERVICE_EXISTS="
sc qc %SERVICE_NAME% >nul 2>&1
if %ERRORLEVEL%==0 (
    rem Stop first so binPath/start-type changes take effect on next start.
    rem Ignore errors: already-stopped / stop-pending return non-zero.
    net stop %SERVICE_NAME% >nul 2>&1
    set SC_CMD=config
    set SERVICE_EXISTS=1
) else (
    set SC_CMD=create
)

rem Preserve the live start type of an existing service. This script re-runs on
rem every reinstall and on every silent auto-update, so writing a fixed default
rem here is what used to downgrade an Automatic service to Manual behind the
rem user's back. Read the registry rather than `sc qc`, whose column labels are
rem localized.
if defined SERVICE_EXISTS (
    set "CURRENT_START="
    set "CURRENT_DELAYED="
    for /f "tokens=3" %%a in ('reg query "%SERVICE_REG_KEY%" /v Start 2^>nul ^| findstr /I /C:"REG_DWORD"') do set "CURRENT_START=%%a"
    for /f "tokens=3" %%a in ('reg query "%SERVICE_REG_KEY%" /v DelayedAutoStart 2^>nul ^| findstr /I /C:"REG_DWORD"') do set "CURRENT_DELAYED=%%a"

    if /I "!CURRENT_START!"=="0x2" (
        if /I "!CURRENT_DELAYED!"=="0x1" (
            set SERVICE_START_TYPE=delayed-auto
        ) else (
            set SERVICE_START_TYPE=auto
        )
    ) else if /I "!CURRENT_START!"=="0x3" (
        set SERVICE_START_TYPE=demand
    ) else if /I "!CURRENT_START!"=="0x4" (
        set SERVICE_START_TYPE=disabled
    ) else (
        rem Unreadable or unexpected value: never guess. `sc config` leaves the
        rem start type alone when no start= argument is passed at all.
        set KEEP_START_TYPE=1
    )
)

rem Restore the user's previous start-type choice if a prior uninstall preserved
rem it. Only a fresh registration needs it — an existing service already carries
rem the live value read above — but consume the file either way so a stale copy
rem cannot resurface on some later install.
set "SAVED_START_TYPE="
if exist "%SERVICE_CONFIG_FILE%" (
    if not defined SERVICE_EXISTS (
        for /f "usebackq delims=" %%a in ("%SERVICE_CONFIG_FILE%") do set "SAVED_START_TYPE=%%a"
    )

    del "%SERVICE_CONFIG_FILE%" >nul 2>&1
)

if "!SAVED_START_TYPE!"=="2-delayed" (
    set SERVICE_START_TYPE=delayed-auto
) else if "!SAVED_START_TYPE!"=="2" (
    set SERVICE_START_TYPE=auto
) else if "!SAVED_START_TYPE!"=="3" (
    set SERVICE_START_TYPE=demand
) else if "!SAVED_START_TYPE!"=="4" (
    set SERVICE_START_TYPE=disabled
)

rem An explicit request from the caller wins over everything above.
if defined FORCED_START_TYPE (
    if /I "!FORCED_START_TYPE!"=="auto" (
        set SERVICE_START_TYPE=auto
    ) else if /I "!FORCED_START_TYPE!"=="delayed-auto" (
        set SERVICE_START_TYPE=delayed-auto
    ) else if /I "!FORCED_START_TYPE!"=="demand" (
        set SERVICE_START_TYPE=demand
    ) else if /I "!FORCED_START_TYPE!"=="disabled" (
        set SERVICE_START_TYPE=disabled
    ) else (
        echo ERROR: Unknown --start-type value: "!FORCED_START_TYPE!"
        exit /b 1
    )

    set "KEEP_START_TYPE="
)

if defined KEEP_START_TYPE (
    echo Keeping the current start type of %SERVICE_NAME%.
) else (
    echo Setting service start type to: [!SERVICE_START_TYPE!]
)

rem `sc create` does not accept delayed-auto directly; create as plain auto
rem then upgrade with a second `sc config` below.
set "SC_START_TYPE=!SERVICE_START_TYPE!"
if /I "!SERVICE_START_TYPE!"=="delayed-auto" set "SC_START_TYPE=auto"
set "SC_START_ARG=start= !SC_START_TYPE!"
if defined KEEP_START_TYPE set "SC_START_ARG="

rem binPath= MUST embed literal quotes around the path so the registry
rem ImagePath becomes "C:\Program Files\...\sunshinesvc.exe" — both to
rem survive paths with spaces and to close the unquoted-service-path
rem security gap. The `"\"%SERVICE_BIN%\""` form is what produces that:
rem outer "..." is one cmd token; inner \"...\" become real quotes in the
rem argv that sc.exe receives. Triple-quoting (`"""..."""`) splits on
rem internal spaces and makes sc print its usage banner instead.
sc !SC_CMD! %SERVICE_NAME% binPath= "\"%SERVICE_BIN%\"" !SC_START_ARG! DisplayName= "Sunshine Service"
if errorlevel 1 (
    echo ERROR: Failed to !SC_CMD! %SERVICE_NAME%.
    exit /b 1
)

if not defined KEEP_START_TYPE if /I "!SERVICE_START_TYPE!"=="delayed-auto" (
    sc config %SERVICE_NAME% start= delayed-auto
    if errorlevel 1 (
        echo ERROR: Failed to set delayed auto-start for %SERVICE_NAME%.
        exit /b 1
    )
)

rem Description is metadata only; AV / SCM contention may transiently
rem block it. Never abort install over a missing description string.
sc description %SERVICE_NAME% "Sunshine is a self-hosted game stream host for Moonlight." >nul 2>&1

if defined SKIP_START (
    echo %SERVICE_NAME% installed without starting; a pending driver operation must finish first.
    exit /b 0
)

if /I "!SERVICE_START_TYPE!"=="disabled" (
    echo %SERVICE_NAME% installed with disabled start type; not starting.
    exit /b 0
)

rem Start the new service. net start returns non-zero when the service is
rem already running (e.g. SCM auto-started it after sc config), so verify the
rem actual state via sc query before treating that as a failure.
net start %SERVICE_NAME%
if errorlevel 1 (
    sc query %SERVICE_NAME% | find /I "RUNNING" >nul
    if errorlevel 1 (
        echo ERROR: Failed to start %SERVICE_NAME%.
        exit /b 1
    )
)

rem NOTE: we deliberately do NOT wait for the Sunshine HTTPS API to be ready
rem here. `sunshinesvc.exe` is just a wrapper that spawns `sunshine.exe` in
rem the active user session via CreateProcessAsUser and reports RUNNING to
rem SCM immediately. The actual `sunshine.exe` first-run setup (config dir,
rem cert generation, audio/video init, HTTPS bind) takes 10-20s, which used
rem to make the installer's "Installing system service..." page appear
rem stuck for ~30s while we polled localhost:47990. Nothing downstream of
rem this script depends on the API being ready — VerifyServiceInstalled()
rem in sunshine.iss only checks the registry ImagePath, and the finish-page
rem GUI button has its own readiness handling. So just exit fast.

exit /b 0
