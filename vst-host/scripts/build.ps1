# ─────────────────────────────────────────────────────────────────────
#  Stratum VST Host build helper (PowerShell)
#
#  Imports the MSVC Build Tools environment into the current PS process
#  (so cl.exe can find its stdlib headers like <algorithm>), then runs
#  the ninja build.
#
#  Usage:  scripts\build.ps1
# ─────────────────────────────────────────────────────────────────────

$ErrorActionPreference = 'Stop'

$VsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $VsDevCmd)) {
    Write-Host "[build.ps1] VsDevCmd.bat not found at:" -ForegroundColor Red
    Write-Host "           $VsDevCmd"
    Write-Host "           Install 'VS 2022 Build Tools' with the C++ workload."
    exit 1
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir    = Join-Path $projectRoot 'build-ninja'

if (-not (Test-Path (Join-Path $buildDir 'build.ninja'))) {
    Write-Host "[build.ps1] No build-ninja\build.ninja — run 'cmake -B build-ninja -G Ninja' first." -ForegroundColor Red
    exit 1
}

# Import the MSVC env into this PowerShell session.
Write-Host "[build.ps1] Importing MSVC environment..."
$envDump = cmd.exe /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}

Write-Host "[build.ps1] Building StratumVSTHost ..."
& cmake --build $buildDir --target StratumVSTHost @args
exit $LASTEXITCODE
