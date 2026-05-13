# watch-juce.ps1
# Watches vst-host/src for changes; on save:
#   1) kills running "Stratum DAW.exe"
#   2) runs an incremental cmake --build (Release)
#   3) relaunches the app
#
# Usage:  npm run watch:juce
#         (or)  pwsh -ExecutionPolicy Bypass -File .\watch-juce.ps1

$ErrorActionPreference = "Stop"

$root      = Split-Path -Parent $MyInvocation.MyCommand.Definition
$srcDir    = Join-Path $root "vst-host\src"
$buildDir  = Join-Path $root "vst-host\build"
$exePath   = Join-Path $buildDir "StratumVSTHost_artefacts\Release\Stratum DAW.exe"
$exeName   = "Stratum DAW"

if (-not (Test-Path $buildDir)) {
    Write-Host "[watch] build/ folder not found. Run cmake configure first." -ForegroundColor Red
    exit 1
}

function Build-And-Run {
    Write-Host ""
    Write-Host "[watch] killing running app (if any)..." -ForegroundColor DarkGray
    Get-Process -Name $exeName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300

    Write-Host "[watch] building (incremental Release)..." -ForegroundColor Cyan
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & cmake --build $buildDir --config Release --target StratumVSTHost 2>&1 |
        Where-Object { $_ -match 'error|Error|warning C[0-9]+|->|Generating|Finished' } |
        Select-Object -Last 8
    $sw.Stop()

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[watch] BUILD FAILED ($([math]::Round($sw.Elapsed.TotalSeconds,1))s) - fix errors and save again" -ForegroundColor Red
        return
    }

    Write-Host "[watch] build OK in $([math]::Round($sw.Elapsed.TotalSeconds,1))s - launching..." -ForegroundColor Green
    Start-Process -FilePath $exePath
}

# initial build + run
Build-And-Run

# debounce: ignore duplicate events that fire within 500ms
$global:lastBuild = [DateTime]::MinValue

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $srcDir
$watcher.IncludeSubdirectories = $true
$watcher.Filter = "*.*"
$watcher.NotifyFilter = [System.IO.NotifyFilters]'LastWrite, FileName, Size'
$watcher.EnableRaisingEvents = $true

$action = {
    $path = $Event.SourceEventArgs.FullPath
    if ($path -notmatch '\.(cpp|h|hpp|cc|cxx)$') { return }

    $now = Get-Date
    if (($now - $global:lastBuild).TotalMilliseconds -lt 500) { return }
    $global:lastBuild = $now

    Write-Host ""
    Write-Host "[watch] change: $([System.IO.Path]::GetFileName($path))" -ForegroundColor Yellow
    Build-And-Run
}

Register-ObjectEvent $watcher Changed -Action $action | Out-Null
Register-ObjectEvent $watcher Created -Action $action | Out-Null
Register-ObjectEvent $watcher Renamed -Action $action | Out-Null

Write-Host ""
Write-Host "[watch] watching $srcDir for .cpp/.h changes..." -ForegroundColor Magenta
Write-Host "[watch] press Ctrl+C to stop" -ForegroundColor DarkGray

try {
    while ($true) { Start-Sleep -Seconds 1 }
}
finally {
    $watcher.Dispose()
}
