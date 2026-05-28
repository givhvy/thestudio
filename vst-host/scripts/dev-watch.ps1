# dev-watch.ps1 — auto rebuild + relaunch Stratum DAW on source changes
#
# Usage (from vst-host/):
#     pwsh -ExecutionPolicy Bypass -File scripts/dev-watch.ps1
#
# What it does:
#   1. Configures a Ninja build dir at vst-host/build-ninja (Debug, first run only).
#   2. Watches src/*.{cpp,h} for changes.
#   3. On change → kills running app → rebuilds → relaunches.

$ErrorActionPreference = 'Stop'

# Resolve repo paths relative to this script's location.
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vstHost   = Resolve-Path (Join-Path $scriptDir '..')
$buildDir  = Join-Path $vstHost 'build-ninja'
$srcDir    = Join-Path $vstHost 'src'

# Locate vcvars64.bat (Visual Studio Build Tools / Community / Professional).
$vcvarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) {
    throw "vcvars64.bat not found. Install Visual Studio (Build Tools or higher) with 'Desktop development with C++'."
}

# Import MSVC env into current PowerShell session (one-shot).
function Import-VsDevEnv {
    Write-Host "[dev-watch] Loading MSVC env from $vcvars" -ForegroundColor DarkGray
    $cmdLine = '"' + $vcvars + '" >nul 2>&1 && set'
    $output = cmd.exe /c $cmdLine
    foreach ($line in $output) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
}
Import-VsDevEnv

# First-time configure (only if build dir doesn't exist yet).
if (-not (Test-Path (Join-Path $buildDir 'build.ninja'))) {
    Write-Host "[dev-watch] Configuring Ninja build (Debug)..." -ForegroundColor Cyan
    cmake -S $vstHost -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Debug | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
}

$exePath = Join-Path $buildDir 'StratumVSTHost_artefacts\Debug\Stratum DAW.exe'

function Build-Then-Launch {
    # Kill any running instance FIRST so the linker can overwrite the .exe.
    $running = Get-Process -Name 'Stratum DAW' -ErrorAction SilentlyContinue
    if ($running) {
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 250
    }

    Write-Host "`n[dev-watch] Building..." -ForegroundColor Yellow
    $sw = [Diagnostics.Stopwatch]::StartNew()
    cmake --build $buildDir --target StratumVSTHost
    $sw.Stop()
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[dev-watch] Build FAILED ($($sw.Elapsed.TotalSeconds.ToString('0.0'))s)" -ForegroundColor Red
        return
    }
    Write-Host "[dev-watch] Build OK in $($sw.Elapsed.TotalSeconds.ToString('0.0'))s - launching..." -ForegroundColor Green

    if (Test-Path $exePath) {
        Start-Process $exePath
    } else {
        Write-Host "[dev-watch] WARN: built exe not found at $exePath" -ForegroundColor DarkYellow
    }
}

# Initial build + launch.
Build-Then-Launch

# Set up FileSystemWatcher on src/.
$fsw = New-Object IO.FileSystemWatcher $srcDir, '*.*'
$fsw.IncludeSubdirectories = $true
$fsw.NotifyFilter = [IO.NotifyFilters]'LastWrite,FileName,Size'
$fsw.EnableRaisingEvents = $true

# Debounce: collect events then trigger one build. Use $global: so the
# FileSystemWatcher event scriptblock (separate scope) can mutate the flag
# the main loop reads.
$global:devWatchPending = $false
$global:devWatchLastAt  = [DateTime]::MinValue

$action = {
    $path = $Event.SourceEventArgs.FullPath
    if ($path -notmatch '\.(cpp|h|hpp|c|cmake|txt)$') { return }
    if ($path -match '\\build') { return }
    $global:devWatchPending = $true
    $global:devWatchLastAt  = Get-Date
}
Register-ObjectEvent $fsw 'Changed' -Action $action | Out-Null
Register-ObjectEvent $fsw 'Created' -Action $action | Out-Null
Register-ObjectEvent $fsw 'Renamed' -Action $action | Out-Null

Write-Host ""
Write-Host "[dev-watch] Watching $srcDir - edit any .cpp / .h to trigger a rebuild." -ForegroundColor Cyan
Write-Host "[dev-watch] Press Ctrl+C to stop." -ForegroundColor DarkGray

try {
    while ($true) {
        Start-Sleep -Milliseconds 250
        if ($global:devWatchPending) {
            $sinceMs = ((Get-Date) - $global:devWatchLastAt).TotalMilliseconds
            if ($sinceMs -ge 400) {
                $global:devWatchPending = $false
                Build-Then-Launch
            }
        }
    }
} finally {
    Get-EventSubscriber | Unregister-Event
    $fsw.Dispose()
}
