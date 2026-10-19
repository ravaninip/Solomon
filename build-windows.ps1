# Solomon - build and install on Windows (VST3)
# Usage (PowerShell, from the project folder):
#   powershell -ExecutionPolicy Bypass -File .\build-windows.ps1
# Options:  -SkipTests   skip the engine tests
#           -InstallDir  destination folder for the .vst3 (default: automatic)
param(
    [switch]$SkipTests,
    [string]$InstallDir = ""
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Step($t) { Write-Host "`n=== $t ===" -ForegroundColor Cyan }
function Fail($t) { Write-Host "`nERROR: $t" -ForegroundColor Red; exit 1 }

# ------------------------------------------------------------------ 1. prerequisites
Step "Checking prerequisites"
$missing = @()

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $missing += "winget install Kitware.CMake" }
if (-not (Get-Command git   -ErrorAction SilentlyContinue)) { $missing += "winget install Git.Git" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$hasVS = $false
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) { $hasVS = $true; Write-Host "C++ compiler found: $vs" }
}
if (-not $hasVS) {
    $missing += 'winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
}

if ($missing.Count -gt 0) {
    Write-Host "Something is missing. Run these commands, then CLOSE and REOPEN PowerShell and run the script again:" -ForegroundColor Yellow
    $missing | ForEach-Object { Write-Host "  $_" }
    exit 1
}

if ($PSScriptRoot.Length -gt 40) {
    Write-Host "Warning: the project path is long ($($PSScriptRoot.Length) characters)." -ForegroundColor Yellow
    Write-Host "If the build fails with path errors, move the folder to C:\dev\Solomon." -ForegroundColor Yellow
}

# ------------------------------------------------------------------ 2. configure and build
Step "Configuring (the first run downloads JUCE from GitHub, internet required)"
cmake -B build -A x64
if ($LASTEXITCODE -ne 0) { Fail "CMake configuration failed (see the messages above)." }

Step "Building (5-15 minutes the first time)"
cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { Fail "build failed (see the messages above)." }

if (-not $SkipTests) {
    Step "Running theory-engine tests"
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { Fail "tests failed: the plugin will not be installed." }
}

# ------------------------------------------------------------------ 3. install
Step "Installing"
$bundle = Get-ChildItem -Path build -Recurse -Directory -Filter "Solomon.vst3" | Select-Object -First 1
if (-not $bundle) { Fail "Solomon.vst3 not found in the build folder." }

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($InstallDir -eq "") {
    if ($isAdmin) { $InstallDir = Join-Path $env:CommonProgramFiles "VST3" }
    else          { $InstallDir = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3" }
}
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$dest = Join-Path $InstallDir "Solomon.vst3"
if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
Copy-Item -Recurse -Force $bundle.FullName $dest

Write-Host "`nDONE. Plugin installed to:" -ForegroundColor Green
Write-Host "  $dest"
Write-Host "`nIn Ableton Live 12: Settings > Plug-Ins > enable 'Use VST3 Plug-In System Folders'"
Write-Host "and press 'Rescan' (hold Alt for a full rescan)."
if ($InstallDir -notlike "$env:CommonProgramFiles*") {
    Write-Host "If Live does not see it, add this as a VST3 custom folder: $InstallDir" -ForegroundColor Yellow
}
