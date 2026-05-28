# Chordify automation setup for Stratum DAW
# Run: powershell -ExecutionPolicy Bypass -File .\vst-host\analysis\setup-chordify.ps1

$ErrorActionPreference = "Stop"
$AnalysisDir = $PSScriptRoot
$VenvDir = Join-Path $AnalysisDir ".venv"
$Python = Join-Path $VenvDir "Scripts\python.exe"
$Pip = Join-Path $VenvDir "Scripts\pip.exe"
$Script = Join-Path $AnalysisDir "chordify_automation.py"

Write-Host "=== Stratum Chordify Automation Setup ===" -ForegroundColor Cyan

if (-not (Test-Path $VenvDir)) {
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
        Write-Error "Python 3.10+ required. Run setup-analysis.ps1 first or install Python."
    }
    python -m venv $VenvDir
}

Write-Host "Installing Playwright..."
& $Pip install playwright
& $Python -m playwright install chromium

Write-Host ""
Write-Host "Next: log in to Chordify Premium (MIDI export required):" -ForegroundColor Yellow
Write-Host "  & `"$Python`" `"$Script`" --login"
Write-Host ""
Write-Host "After login, drag loops to Playlist - Stratum auto-uploads to Chordify and imports bass MIDI."
