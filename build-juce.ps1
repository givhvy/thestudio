# Build script for JUCE WebBrowserComponent hybrid app
# Run this from PowerShell: .\build-juce.ps1

$ErrorActionPreference = "Stop"

Write-Host "========================================="
Write-Host "  Building Stratum DAW (JUCE Hybrid)"
Write-Host "========================================="

# Step 1: Build frontend
Write-Host "`n[1/4] Building frontend..." -ForegroundColor Cyan
Set-Location $PSScriptRoot\frontend
npm run build 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Frontend build failed. Make sure Node.js is installed." -ForegroundColor Red
    exit 1
}
Write-Host "Frontend built successfully." -ForegroundColor Green

# Step 2: Find CMake
Write-Host "`n[2/4] Looking for CMake..." -ForegroundColor Cyan
$cmake = $null

# Check PATH first
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    $cmake = $cmake.Source
}

# Check Visual Studio bundled CMake
if (-not $cmake) {
    $vsPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($p in $vsPaths) {
        if (Test-Path $p) {
            $cmake = $p
            break
        }
    }
}

# Check standalone CMake
if (-not $cmake) {
    $standalonePaths = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\CMake\bin\cmake.exe"
    )
    foreach ($p in $standalonePaths) {
        if (Test-Path $p) {
            $cmake = $p
            break
        }
    }
}

if (-not $cmake) {
    Write-Host "CMake not found! Please install:" -ForegroundColor Red
    Write-Host "  1. Visual Studio Build Tools 2022 with 'C++ build tools' workload" -ForegroundColor Yellow
    Write-Host "  2. OR download CMake from https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found CMake: $cmake" -ForegroundColor Green

# Step 3: Configure
Write-Host "`n[3/4] Configuring JUCE project..." -ForegroundColor Cyan
Set-Location $PSScriptRoot\vst-host

# Clean old build if it was for console app
if (Test-Path build\CMakeCache.txt) {
    $cacheContent = Get-Content build\CMakeCache.txt -Raw
    if ($cacheContent -notmatch "juce_gui_extra" -or $cacheContent -match "juce_add_console_app") {
        Write-Host "Old console-app build detected. Cleaning..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    }
}

& $cmake -B build -S . -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed." -ForegroundColor Red
    exit 1
}
Write-Host "CMake configuration succeeded." -ForegroundColor Green

# Step 4: Build
Write-Host "`n[4/4] Building Release binary..." -ForegroundColor Cyan
& $cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

Write-Host "`n=========================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "========================================="
Write-Host "Binary: .\vst-host\build\Release\StratumVSTHost.exe" -ForegroundColor Cyan
Write-Host "`nTo run:" -ForegroundColor Yellow
Write-Host "  .\vst-host\build\Release\StratumVSTHost.exe" -ForegroundColor White
