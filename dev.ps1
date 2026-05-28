# Stratum DAW — Dev watcher
# Watches frontend/src for changes, rebuilds with Vite, restarts JUCE app.
# The JUCE bridge works correctly in prod mode (resource provider).
#
# Usage: .\dev.ps1

$root = $PSScriptRoot
$frontendSrc = "$root\frontend\src"
$exe = "$root\vst-host\build\StratumVSTHost_artefacts\Release\Stratum DAW.exe"
$appProcess = $null

function Invoke-BuildAndLaunch {
    Write-Host "`n[DEV] Building frontend..." -ForegroundColor Cyan
    & npm run build 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "[DEV] Build failed!" -ForegroundColor Red; return }
    Write-Host "[DEV] Build done. Relaunching app..." -ForegroundColor Green

    # Kill old instance
    Get-Process -Name "Stratum DAW" -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 400

    $script:appProcess = Start-Process -FilePath $exe -PassThru
    Write-Host "[DEV] App launched (PID $($script:appProcess.Id))" -ForegroundColor Green
}

Write-Host "[DEV] Stratum DAW watcher started." -ForegroundColor Cyan
Write-Host "[DEV] Watching: $frontendSrc" -ForegroundColor Yellow
Write-Host "[DEV] Press Ctrl+C to stop.`n" -ForegroundColor Yellow

# Initial build + launch
Set-Location "$root\frontend"
Build-And-Launch

# Watch for file changes
$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $frontendSrc
$watcher.Filter = "*.*"
$watcher.IncludeSubdirectories = $true
$watcher.EnableRaisingEvents = $true
$watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite

$debounceTimer = $null
$onChange = {
    if ($script:debounceTimer) { $script:debounceTimer.Stop() }
    $script:debounceTimer = [System.Timers.Timer]::new(800)
    $script:debounceTimer.AutoReset = $false
    $script:debounceTimer.Add_Elapsed({ Invoke-BuildAndLaunch })
    $script:debounceTimer.Start()
}

Register-ObjectEvent $watcher Changed -Action $onChange | Out-Null
Register-ObjectEvent $watcher Created -Action $onChange | Out-Null

try {
    while ($true) { Start-Sleep -Seconds 1 }
} finally {
    $watcher.EnableRaisingEvents = $false
    Get-Process -Name "Stratum DAW" -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Host "`n[DEV] Stopped." -ForegroundColor Yellow
}
