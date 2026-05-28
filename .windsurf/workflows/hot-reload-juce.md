---
description: Set up Ninja-based hot-reload (auto rebuild + relaunch) for a JUCE/CMake C++ app on Windows
---

# Hot-reload for JUCE/CMake apps (Windows + Ninja + PowerShell)

Goal: edit a `.cpp` / `.h` → save → app rebuilds and relaunches in ~5–10 s.
Replaces slow MSBuild Release rebuilds (~60–90 s).

This workflow assumes:
- Project uses CMake.
- Visual Studio 2022 Build Tools (or Community/Pro/Enterprise) is installed with "Desktop C++".
- The app produces a single executable target (let it be `<TARGET>`, e.g. `StratumVSTHost`).

## 1. Install Ninja

// turbo
```powershell
winget install --id Ninja-build.Ninja --accept-source-agreements --accept-package-agreements
```

After install, restart the shell so `ninja` is on PATH.

## 2. Locate `vcvars64.bat`

Find it at one of these paths:

```
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat
```

Confirm via:
```powershell
Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio\2022','C:\Program Files\Microsoft Visual Studio\2022' -Filter vcvars64.bat -Recurse -EA Silent | Select-Object FullName
```

## 3. Configure a Ninja build dir (one-time)

Use a **separate** dir from any existing MSBuild dir (e.g. `build-ninja/`).
You must run `cmake -G Ninja` from a shell where `cl.exe` is on PATH — easiest is to import `vcvars64.bat`:

```powershell
$vc = '<path to vcvars64.bat>'
$envLines = cmd.exe /c ('"' + $vc + '" >nul 2>&1 && set')
foreach ($l in $envLines) { if ($l -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') } }

cmake -S <project-dir> -B <project-dir>\build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Cold compile after this is similar to MSBuild (~30 s for a JUCE app); incremental will be ~5–10 s.

Add `<project-dir>/build-ninja/` to `.gitignore`.

## 4. Create the watcher script

Create `<project-dir>/scripts/dev-watch.ps1`. Replace `<TARGET>` with your CMake target name and `<EXE NAME>` with the produced exe filename (e.g. `Stratum DAW.exe`).

```powershell
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projDir   = Resolve-Path (Join-Path $scriptDir '..')
$buildDir  = Join-Path $projDir 'build-ninja'
$srcDir    = Join-Path $projDir 'src'

$vcvarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "vcvars64.bat not found." }

# Import MSVC env (use single-quote concat — backtick-escaped quotes break PS 5.1 parsing).
$cmdLine = '"' + $vcvars + '" >nul 2>&1 && set'
foreach ($l in (cmd.exe /c $cmdLine)) {
    if ($l -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}

if (-not (Test-Path (Join-Path $buildDir 'build.ninja'))) {
    cmake -S $projDir -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Debug | Out-Host
}

$exePath = Join-Path $buildDir '<TARGET>_artefacts\Debug\<EXE NAME>'
$procName = '<EXE NAME>' -replace '\.exe$',''

function Build-Then-Launch {
    # Kill running app FIRST so the linker can overwrite the .exe (LNK1168 otherwise).
    $running = Get-Process -Name $procName -EA Silent
    if ($running) { $running | Stop-Process -Force; Start-Sleep -Milliseconds 250 }

    $sw = [Diagnostics.Stopwatch]::StartNew()
    cmake --build $buildDir --target <TARGET>
    $sw.Stop()
    if ($LASTEXITCODE -ne 0) { Write-Host "Build FAILED ($($sw.Elapsed.TotalSeconds.ToString('0.0'))s)" -ForegroundColor Red; return }
    Write-Host "Build OK in $($sw.Elapsed.TotalSeconds.ToString('0.0'))s - launching..." -ForegroundColor Green
    if (Test-Path $exePath) { Start-Process $exePath }
}

Build-Then-Launch

$fsw = New-Object IO.FileSystemWatcher $srcDir, '*.*'
$fsw.IncludeSubdirectories = $true
$fsw.NotifyFilter = [IO.NotifyFilters]'LastWrite,FileName,Size'
$fsw.EnableRaisingEvents = $true

# IMPORTANT: use $global: — FSW event scriptblock runs in a separate scope,
# so $script:foo updates are NOT visible to the main loop.
$global:devWatchPending = $false
$global:devWatchLastAt  = [DateTime]::MinValue

$action = {
    $p = $Event.SourceEventArgs.FullPath
    if ($p -notmatch '\.(cpp|h|hpp|c|cmake|txt)$') { return }
    if ($p -match '\\build') { return }
    $global:devWatchPending = $true
    $global:devWatchLastAt  = Get-Date
}
Register-ObjectEvent $fsw 'Changed' -Action $action | Out-Null
Register-ObjectEvent $fsw 'Created' -Action $action | Out-Null
Register-ObjectEvent $fsw 'Renamed' -Action $action | Out-Null

try {
    while ($true) {
        Start-Sleep -Milliseconds 250
        if ($global:devWatchPending) {
            if (((Get-Date) - $global:devWatchLastAt).TotalMilliseconds -ge 400) {
                $global:devWatchPending = $false
                Build-Then-Launch
            }
        }
    }
} finally {
    Get-EventSubscriber | Unregister-Event
    $fsw.Dispose()
}
```

## 5. Save the script as UTF-8 **with BOM**

Windows PowerShell 5.1 reads BOM-less files as Windows-1252 and may produce phantom parse errors like `Missing closing '}' in statement block` even when syntax is correct.

```powershell
$content = [IO.File]::ReadAllText('<project-dir>\scripts\dev-watch.ps1')
[IO.File]::WriteAllText('<project-dir>\scripts\dev-watch.ps1', $content, (New-Object System.Text.UTF8Encoding($true)))
```

Verify the first 3 bytes are `EF BB BF`:

```powershell
([IO.File]::ReadAllBytes('<project-dir>\scripts\dev-watch.ps1')[0..2]) -join ' '
```

## 6. Run it

```powershell
powershell -NoLogo -ExecutionPolicy Bypass -File <project-dir>\scripts\dev-watch.ps1
```

Edit a `.cpp` / `.h` and save. Within ~7 s the app rebuilds and relaunches.

## 7. End-to-end smoke test

```powershell
$proc = Start-Process powershell -ArgumentList '-NoLogo','-ExecutionPolicy','Bypass','-File','<project-dir>\scripts\dev-watch.ps1' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$env:TEMP\dev-watch.log" -RedirectStandardError "$env:TEMP\dev-watch.err"
Start-Sleep 12
$before = (Get-Process -Name '<EXE basename>' -EA Silent).Id
(Get-Item '<project-dir>\src\<some-file>.cpp').LastWriteTime = Get-Date
Start-Sleep 18
$after = (Get-Process -Name '<EXE basename>' -EA Silent).Id
if ($after -and $after -ne $before) { "PASS: $before -> $after" } else { "FAIL"; Get-Content "$env:TEMP\dev-watch.log" -Tail 20 }
```

## Common pitfalls and fixes

- **`Missing closing '}' in statement block`** while the script is syntactically correct → the file lacks a UTF-8 BOM. Re-save it with BOM (step 5).
- **`LNK1168: cannot open <exe> for writing`** → the previous app instance is still holding the file open. The script kills it BEFORE invoking `cmake --build`; do not move that kill to after.
- **Script triggers but state never updates** → you used `$script:foo` inside the FSW event scriptblock. That runs in a separate scope. Use `$global:foo` for the flag and timestamp.
- **`pwsh` not found** when launching watcher → use `powershell` (Windows PS 5.1) instead, or install PowerShell 7.
- **`cl.exe` / `link.exe` not found by Ninja** → you didn't import `vcvars64.bat` into the session. The script does this automatically; if running cmake manually, do it first.
- **Cold rebuild instead of incremental** → you re-ran `cmake -G Ninja` which can clear the cache. Only re-configure when CMakeLists.txt changes; otherwise just `cmake --build <dir>`.

## Expected timings (JUCE app, ~30 source files)

- First-time configure: ~30 s
- Cold build: ~30 s
- Incremental rebuild (1 .cpp changed): **5–8 s**
- Incremental rebuild (1 .h changed, many includers): 15–40 s
