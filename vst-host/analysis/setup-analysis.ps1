# Stratum ML Bass Extractor Setup
# Run: powershell -ExecutionPolicy Bypass -File .\vst-host\analysis\setup-analysis.ps1

$ErrorActionPreference = "Stop"
$AnalysisDir = $PSScriptRoot
$ThirdParty = Join-Path $AnalysisDir "third_party"
$LiveChordDir = Join-Path $ThirdParty "LiveChord"
$ChordMiniDir = Join-Path $ThirdParty "ChordMini"
$VenvDir = Join-Path $AnalysisDir ".venv"
$Python = Join-Path $VenvDir "Scripts\python.exe"
$Pip = Join-Path $VenvDir "Scripts\pip.exe"
$ModelPath = Join-Path $LiveChordDir "backend\btc\btc_model_large_voca.pt"
$ChordMiniModel = Join-Path $ChordMiniDir "checkpoints\btc_model_large_voca.pt"

Write-Host "=== Stratum ML Bass Extractor Setup ===" -ForegroundColor Cyan

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Error "Python 3.10+ is required."
}

if (-not (Test-Path $VenvDir)) {
    Write-Host "Creating virtual environment..."
    python -m venv $VenvDir
}

Write-Host "Upgrading pip..."
& $Python -m pip install --upgrade pip setuptools wheel

Write-Host "Installing PyTorch (CPU)..."
& $Pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu

Write-Host "Installing analysis requirements..."
& $Pip install -r (Join-Path $AnalysisDir "requirements.txt")

if (-not (Test-Path $LiveChordDir)) {
    Write-Host "Cloning LiveChord (BTC backend code)..."
    New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null
    git clone --depth 1 https://github.com/JJ110112/LiveChord.git $LiveChordDir
}

if (-not (Test-Path $ChordMiniDir)) {
    Write-Host "Cloning ChordMini (BTC large-voca checkpoint)..."
    New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null
    git clone --depth 1 https://github.com/ptnghia-j/ChordMini.git $ChordMiniDir
}

if (-not (Test-Path $ModelPath)) {
    if (Test-Path $ChordMiniModel) {
        Write-Host "Installing BTC large-voca checkpoint..."
        New-Item -ItemType Directory -Force -Path (Split-Path $ModelPath -Parent) | Out-Null
        Copy-Item $ChordMiniModel $ModelPath -Force
    }
    else {
        Write-Warning "Missing btc_model_large_voca.pt. Re-run after ChordMini clone completes."
    }
}

if (-not (Test-Path $ModelPath)) {
    Write-Warning "Setup incomplete - BTC model still missing."
    exit 1
}

Write-Host "Setup complete." -ForegroundColor Green
Write-Host ("Python: " + $Python)
Write-Host ("Model:  " + $ModelPath)
