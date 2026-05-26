# Chordify login via Chrome DevTools (CDP) — bypasses Cloudflare bot block
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

Write-Host "Step 1: Starting Chrome with DevTools port..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "launch-chordify-chrome.ps1")

Write-Host ""
Write-Host "Step 2: In the Chrome window:" -ForegroundColor Yellow
Write-Host "  - Pass Cloudflare if shown"
Write-Host "  - Login Chordify Premium"
Write-Host "  - You should see Upload Song on chordify.net"
Write-Host ""
Read-Host "Press Enter when logged in"

Write-Host ""
Write-Host "Step 3: Saving session for Stratum..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot ".venv\Scripts\python.exe") (Join-Path $PSScriptRoot "chordify_automation.py") --login --use-cdp

Write-Host ""
Write-Host "Done. Keep Chrome open. Drag loops into Stratum Playlist." -ForegroundColor Green
Read-Host "Press Enter to close"
