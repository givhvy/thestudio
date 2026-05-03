# Building the Stratum VST Host (No Visual Studio IDE required)

This guide uses only **free command-line tools** + **VS Code**.
Total install size: ~4.5 GB. Time: ~15 min to install, ~5 min to build.

---

## 1. Install MSVC Build Tools (compiler only, no IDE)

1. Download the **Build Tools** installer:
   https://aka.ms/vs/17/release/vs_BuildTools.exe

2. Run it and select **only** these workloads:
   - ✅ **C++ build tools**
   - ✅ **MSVC v143** (latest)
   - ✅ **Windows 11 SDK** (or Windows 10 SDK)
   - ✅ **CMake tools for Windows** (optional — you can install CMake separately)

3. Click Install. No Visual Studio IDE is installed.

---

## 2. Install CMake

Download the **Windows x64 Installer** from:
https://cmake.org/download/

During install, choose **"Add CMake to the system PATH for all users"**.

---

## 3. Install Git (if not already)

https://git-scm.com/download/win

---

## 4. Install VS Code extensions

Open VS Code and install:
- `ms-vscode.cmake-tools`  (CMake Tools)
- `ms-vscode.cpptools`     (C/C++ IntelliSense)

---

## 5. Clone JUCE

Open a terminal inside the `vst-host` folder and run:

```bash
git clone --depth 1 https://github.com/juce-framework/JUCE.git JUCE
```

This clones JUCE alongside the CMakeLists.txt. It will be ~200 MB.

---

## 6. Configure the build (VS Code way)

1. Open the `vst-host` folder in VS Code:
   ```
   File → Open Folder → f:\PlaygroundTest\flstudioclonee\vst-host
   ```

2. Press **Ctrl+Shift+P** → `CMake: Select a Kit`
   → Choose **Visual Studio Build Tools 2022 Release - amd64**

3. Press **Ctrl+Shift+P** → `CMake: Configure`

4. Press **Ctrl+Shift+P** → `CMake: Build`
   (or press **F7**)

The output binary will be at:
```
vst-host\build\Release\StratumVSTHost.exe
```

---

## 7. Configure the build (terminal way)

Open a **Developer Command Prompt for VS 2022 Build Tools** (search Start menu) or run from a regular PowerShell after running:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
```

Then:

```bash
cd f:\PlaygroundTest\flstudioclonee\vst-host
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## 8. Verify it works

```bash
.\build\Release\StratumVSTHost.exe
```

You should see:
```
JsonRpcServer: listening on port 9001
StratumVSTHost ready. JSON-RPC port 9001
```

Then in Electron/Node, run:
```js
const vst = require('./electron/vstBridge');
await vst.connect();
const result = await vst.call('ping');
// result === "pong"
```

---

## 9. Scan and load plugins

```js
// Scan a folder of VST3 plugins
const plugins = await vst.call('scanPlugins', { path: 'C:/Program Files/Common Files/VST3' });

// Load a plugin by name
const { slotId } = await vst.call('loadPlugin', { fileOrIdentifier: 'Serum.vst3' });

// Show its GUI window
await vst.call('showEditor', { slotId, show: true });

// Send MIDI note
await vst.call('noteOn', { slotId, channel: 1, note: 60, velocity: 100 });
await vst.call('noteOff', { slotId, channel: 1, note: 60 });
```

---

## Supported plugin formats

| Format | Extension | Works |
|--------|-----------|-------|
| VST3   | `.vst3`   | ✅ Yes |
| VST2   | `.dll`    | ✅ Yes (needs Steinberg VST2 SDK header in JUCE) |
| AU     | macOS only| macOS only |
| CLAP   | `.clap`   | Requires adding CLAP SDK to CMakeLists.txt |

---

## Troubleshooting

- **"JUCE not found"** — make sure you cloned JUCE into `vst-host/JUCE/`
- **"Cannot find compiler"** — open a Developer Command Prompt, not regular PowerShell
- **Plugin not scanning** — make sure the folder contains valid `.vst3` bundles or `.dll` files
- **Audio device error** — ensure no other app has exclusive audio device lock (close DAW software)
