<#
.SYNOPSIS
    System Health Toolkit — PowerShell Launcher
.DESCRIPTION
    Cross-platform PowerShell launcher for the System Health Toolkit.
    Detects OS, locates the binary, and launches with proper arguments.
    Supports auto-elevation on Windows.
.PARAMETER AutoRepair
    Run automatic repair and exit
.PARAMETER NoColor
    Disable colored output
.PARAMETER NoExport
    Do not auto-export reports
.PARAMETER LogDir
    Custom log directory
.PARAMETER ReportDir
    Custom report directory
.PARAMETER Help
    Show help
.EXAMPLE
    .\launcher.ps1
    .\launcher.ps1 -AutoRepair
    .\launcher.ps1 -NoColor -NoExport
#>

param(
    [switch]$AutoRepair,
    [switch]$NoColor,
    [switch]$NoExport,
    [string]$LogDir = "",
    [string]$ReportDir = "",
    [switch]$Help
)

$SHT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$SHT_EXE = ""

# Determine the binary name based on platform
$isWindows = $env:OS -match "Windows"
if ($isWindows) {
    $binaryName = "SystemHealthToolkit.exe"
} else {
    $binaryName = "SystemHealthToolkit"
}

# Search paths (newest build location first).
# Note: multi-segment Join-Path requires PS 7+; keep 5.1-compatible.
$searchPaths = @(
    (Join-Path $SHT_DIR "build_cmake\Release\$binaryName"),
    (Join-Path $SHT_DIR "build\Release\$binaryName"),
    (Join-Path $SHT_DIR "build\$binaryName"),
    (Join-Path $SHT_DIR "bin\$binaryName"),
    (Join-Path $SHT_DIR $binaryName),
    (Join-Path $SHT_DIR "build\Debug\$binaryName")
)

foreach ($p in $searchPaths) {
    if (Test-Path $p) {
        $SHT_EXE = $p
        break
    }
}

# Try PATH
if (-not $SHT_EXE) {
    $which = Get-Command $binaryName -ErrorAction SilentlyContinue
    if ($which) { $SHT_EXE = $which.Source }
}

# Build if not found. CMake's default generator locates an installed
# Visual Studio by itself — no compiler needs to be on PATH.
if (-not $SHT_EXE) {
    Write-Host "[SHT] Executable not found. Building from source..." -ForegroundColor Yellow
    Write-Host ""

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Host "[SHT] ERROR: CMake is not installed." -ForegroundColor Red
        Write-Host "Install it with:  winget install --id Kitware.CMake -e" -ForegroundColor Yellow
        Write-Host "or download from https://cmake.org/download/" -ForegroundColor Yellow
        exit 1
    }

    $buildDir = Join-Path $SHT_DIR "build_cmake"
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    Push-Location $buildDir

    cmake .. -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        Write-Host "[SHT] ERROR: CMake configuration failed - no usable C++ compiler found." -ForegroundColor Red
        Write-Host "Install one with:  winget install --id Microsoft.VisualStudio.2022.BuildTools -e" -ForegroundColor Yellow
        Write-Host "(select the 'Desktop development with C++' workload)" -ForegroundColor Yellow
        exit 1
    }
    cmake --build . --config Release --target SystemHealthToolkit
    Pop-Location

    foreach ($p in $searchPaths) {
        if (Test-Path $p) { $SHT_EXE = $p; break }
    }
}

if (-not (Test-Path $SHT_EXE)) {
    Write-Host "[SHT] ERROR: Could not find or build the executable." -ForegroundColor Red
    exit 1
}

# Create directories
$dirs = @("Logs", "Reports", "Temp", "Config", "Assets")
foreach ($d in $dirs) {
    $path = Join-Path $SHT_DIR $d
    if (-not (Test-Path $path)) { New-Item -ItemType Directory -Path $path | Out-Null }
}

# Build arguments
$argsList = @()
if ($AutoRepair) { $argsList += "--auto-repair" }
if ($NoColor)    { $argsList += "--no-color" }
if ($NoExport)   { $argsList += "--no-export" }
if ($LogDir)     { $argsList += "--log-dir"; $argsList += $LogDir }
if ($ReportDir)  { $argsList += "--report-dir"; $argsList += $ReportDir }
if ($Help)       { $argsList += "--help" }

# Check elevation on Windows
if ($isWindows) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
} else {
    $elevated = (id -u) -eq 0
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  SYSTEM HEALTH TOOLKIT  v3.0.0  -  PowerShell Launcher" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Launcher:  $($MyInvocation.MyCommand.Name)" -ForegroundColor Gray
Write-Host "Executable: $SHT_EXE" -ForegroundColor Gray
Write-Host "Elevated:   $(if($elevated){'Yes'}else{'No'})" -ForegroundColor Gray
Write-Host ""

# Launch
Write-Host "[SHT] Starting System Health Toolkit..." -ForegroundColor Green
Write-Host ""

# Start-Process errors on an empty -ArgumentList in PS 5.1
if ($argsList.Count -gt 0) {
    $process = Start-Process -FilePath $SHT_EXE -ArgumentList $argsList -NoNewWindow -Wait -PassThru
} else {
    $process = Start-Process -FilePath $SHT_EXE -NoNewWindow -Wait -PassThru
}
if ($process.ExitCode -ne 0) {
    Write-Host ""
    Write-Host "[SHT] The toolkit exited with code $($process.ExitCode)." -ForegroundColor Red
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  - Run as Administrator for full functionality" -ForegroundColor Yellow
    Write-Host "  - Check Logs\Errors.log for details" -ForegroundColor Yellow
}