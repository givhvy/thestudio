# Launch Google Chrome with DevTools port for Stratum Chordify automation.
# Cloudflare blocks Playwright browsers — real Chrome + CDP works.

$ErrorActionPreference = "Stop"

$ProfileDir = Join-Path $env:LOCALAPPDATA "StratumChordify\ChromeProfile"
$Port = 9222
New-Item -ItemType Directory -Force -Path $ProfileDir | Out-Null

$ChromeCandidates = @(
    "${env:ProgramFiles}\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
    "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe"
)

$Chrome = $ChromeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Chrome) {
    Write-Error "Google Chrome not found. Install Chrome from https://www.google.com/chrome/"
}

# Kill stale Stratum Chordify Chrome on same port (optional)
Get-Process chrome -ErrorAction SilentlyContinue | Out-Null

Write-Host "=== Stratum Chordify Chrome ===" -ForegroundColor Cyan
Write-Host "Profile: $ProfileDir"
Write-Host "CDP port: $Port"
Write-Host ""
Write-Host "1. Chrome opens chordify.net"
Write-Host "2. Pass Cloudflare + login Premium if needed"
Write-Host "3. Keep this Chrome window OPEN while using Stratum"
Write-Host ""

Start-Process -FilePath $Chrome -ArgumentList @(
    "--remote-debugging-port=$Port",
    "--user-data-dir=`"$ProfileDir`"",
    "--no-first-run",
    "https://chordify.net/"
)

Write-Host "Chrome started. Run run-chordify-login.ps1 when logged in." -ForegroundColor Green
