$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$pluginDir = Join-Path $root "vst-plugins"

cmake -S $pluginDir -B (Join-Path $pluginDir "build")
cmake --build (Join-Path $pluginDir "build") --config Release

Write-Host "Built plugins into: $pluginDir\_installed\VST3"
