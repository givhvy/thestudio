# Stratum DAW — Dev mode launcher
# Starts Vite HMR server + JUCE app pointed at it.
# Frontend changes hot-reload instantly. Only rebuild C++ when .cpp/.h files change.
#
# Usage: .\dev.ps1

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "`n[DEV] Starting Vite dev server on port 3001..." -ForegroundColor Cyan
$vite = Start-Process -FilePath "npm" -ArgumentList "run", "dev" -WorkingDirectory "$root\frontend" -PassThru -NoNewWindow

Write-Host "[DEV] Waiting for Vite to start..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

Write-Host "[DEV] Launching Stratum DAW in dev mode..." -ForegroundColor Cyan
$env:STRATUM_DEV = "1"
$env:STRATUM_DEV_PORT = "3001"

$exe = "$root\vst-host\build\StratumVSTHost_artefacts\Release\Stratum DAW.exe"
if (-not (Test-Path $exe)) {
    Write-Host "[DEV] Binary not found. Building first..." -ForegroundColor Yellow
    Set-Location "$root\vst-host"
    cmake --build build --config Release
}

Start-Process -FilePath $exe

Write-Host "`n[DEV] Running! Edit frontend/src/**/*.jsx and changes hot-reload." -ForegroundColor Green
Write-Host "[DEV] Press Ctrl+C to stop Vite server." -ForegroundColor Yellow

# Keep script alive so Vite stays running
try {
    Wait-Process -Id $vite.Id
} catch {
    # Vite exited or user pressed Ctrl+C
}
