<#
.SYNOPSIS
Build Foundation Sunshine from this checkout and update the installed copy in place.

.DESCRIPTION
Runs the same steps as CI (MSYS2 UCRT64 + Ninja build, Web UI via npm, Inno Setup
packaging) and then runs the produced installer silently with the switches the GUI
updater uses. The installer keeps the existing {app}\config directory, so pairings,
apps and settings survive the update. The Sunshine service is stopped and restarted by
the installer, so any running stream ends.

Requirements:
  - MSYS2 with the UCRT64 packages from .github/workflows/main.yml (docs/building.md)
  - Windows Node.js (npm on PATH) for the Web UI build
  - Inno Setup 6
  - .NET SDK 10, only when build\ds5-sidecar-package.json has to be (re)generated
  - GITHUB_TOKEN in the environment if you want the private vmouse driver packaged;
    without it that driver is skipped and everything else (ZakoVDD included) still builds

.PARAMETER BuildOnly
Build and package, but do not run the installer.

.PARAMETER RebuildSidecar
Regenerate the DualSense sidecar manifest even if build\ds5-sidecar-package.json exists.

.PARAMETER SidecarReleaseTag
Release tag written into the sidecar manifest download URL. Defaults to the newest git tag.

.PARAMETER Msys2Root
MSYS2 installation root. Defaults to C:\msys64.

.PARAMETER Jobs
Parallel ninja jobs. 0 lets ninja decide.

.EXAMPLE
.\update-in-place.ps1
.EXAMPLE
.\update-in-place.ps1 -BuildOnly
#>
[CmdletBinding()]
param(
    [switch]$BuildOnly,
    [switch]$RebuildSidecar,
    [string]$SidecarReleaseTag = "",
    [string]$Msys2Root = "C:\msys64",
    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Set-Location $root

function Write-Step([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function ConvertTo-MsysPath([string]$WindowsPath) {
    $p = $WindowsPath -replace '\\', '/'
    if ($p -match '^([A-Za-z]):/(.*)$') {
        return "/$($Matches[1].ToLower())/$($Matches[2])"
    }
    return $p
}

function Invoke-Ucrt64([string]$Command) {
    $env:MSYSTEM = 'UCRT64'
    $env:CHERE_INVOKING = '1'
    # Keep the Windows PATH so npm, dotnet and ISCC stay visible inside the shell.
    $env:MSYS2_PATH_TYPE = 'inherit'
    & $bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        Fail "Command failed in the UCRT64 shell (exit $LASTEXITCODE): $Command"
    }
}

# --- Preconditions ---------------------------------------------------------------
$bash = Join-Path $Msys2Root 'usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $bash)) {
    Fail "MSYS2 not found at $Msys2Root. Install it from https://www.msys2.org or pass -Msys2Root."
}
if (-not (Test-Path -LiteralPath (Join-Path $Msys2Root 'ucrt64\bin\gcc.exe'))) {
    Fail "UCRT64 toolchain missing. In the MSYS2 UCRT64 shell run the pacman line from .github/workflows/main.yml."
}
if (-not (Get-Command npm.cmd -ErrorAction SilentlyContinue)) {
    Fail "Windows Node.js (npm) is not on PATH. The Web UI build needs it."
}
$isccCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
)
$iscc = $isccCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $iscc) {
    Fail "Inno Setup 6 (ISCC.exe) not found. Install it from https://jrsoftware.org/isdl.php"
}
if (-not (Test-Path -LiteralPath (Join-Path $root 'third-party\moonlight-common-c\src'))) {
    Write-Step "Fetching submodules"
    & git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { Fail "git submodule update failed" }
}

# --- DualSense sidecar manifest (required by the installer) -----------------------
$manifest = Join-Path $root 'build\ds5-sidecar-package.json'
if ($RebuildSidecar -or -not (Test-Path -LiteralPath $manifest)) {
    if ([string]::IsNullOrWhiteSpace($SidecarReleaseTag)) {
        $SidecarReleaseTag = (& git tag --sort=-v:refname | Select-Object -First 1)
        if ([string]::IsNullOrWhiteSpace($SidecarReleaseTag)) { $SidecarReleaseTag = 'local' }
    }
    Write-Step "Generating the DualSense sidecar manifest (tag $SidecarReleaseTag, needs the .NET SDK)"
    & (Join-Path $root 'scripts\build-ds5-sidecar.ps1') -ReleaseTag $SidecarReleaseTag
    & (Join-Path $root 'scripts\package-ds5-sidecar.ps1') -ReleaseTag $SidecarReleaseTag
}
else {
    Write-Step "Reusing sidecar manifest $manifest (pass -RebuildSidecar to regenerate)"
}

# --- Configure and build ----------------------------------------------------------
$rootUnix = ConvertTo-MsysPath $root
$isccCmake = $iscc -replace '\\', '/'
$driverDepsRequired = if ($env:GITHUB_TOKEN) { 'ON' } else { 'OFF' }
if ($driverDepsRequired -eq 'OFF') {
    Write-Host "GITHUB_TOKEN not set: private driver downloads (vmouse) are optional for this build." -ForegroundColor Yellow
}

$configure = @(
    'cmake -B build -G Ninja -S .',
    '-DBUILD_DOCS=OFF',
    '-DBUILD_TESTS=OFF',
    '-DBUILD_TRAY_TESTS=OFF',
    '-DSUNSHINE_ASSETS_DIR=assets',
    "-DDRIVER_DEPS_REQUIRED=$driverDepsRequired",
    "-DISCC_EXECUTABLE='$isccCmake'"
) -join ' '

$ninjaJobs = ''
if ($Jobs -gt 0) { $ninjaJobs = "-j $Jobs" }

Write-Step "Configuring (UCRT64)"
Invoke-Ucrt64 "cd '$rootUnix' && $configure"

Write-Step "Building sunshine and the Web UI"
Invoke-Ucrt64 "cd '$rootUnix' && ninja -C build $ninjaJobs"

Write-Step "Packaging the installer (staging, strip, ISCC)"
Invoke-Ucrt64 "cd '$rootUnix' && ninja -C build innosetup"

$installer = Join-Path $root 'build\cpack_artifacts\Sunshine.exe'
if (-not (Test-Path -LiteralPath $installer)) {
    Fail "Installer not found at $installer"
}

if ($BuildOnly) {
    Write-Host "Installer ready: $installer" -ForegroundColor Green
    exit 0
}

# --- Install in place -------------------------------------------------------------
Write-Step "Installing in place. The installer preserves {app}\config, so pairings and settings stay."
$proc = Start-Process -FilePath $installer `
    -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-' `
    -Verb RunAs -Wait -PassThru

switch ($proc.ExitCode) {
    0 { Write-Host "Update complete. The Sunshine service is running the new build." -ForegroundColor Green }
    3010 { Write-Host "Update complete, but Windows wants a reboot to finish a driver step." -ForegroundColor Yellow }
    default { Fail "Installer exited with code $($proc.ExitCode). See the newest 'Setup Log' file in $env:TEMP." }
}
