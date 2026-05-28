# Stratum VST Plugins

This folder is for VST plugins made for Stratum DAW.

## Build

From this folder:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Or from the repo root:

```powershell
.\build-vst-plugins.ps1
```

Built VST3 plugins are copied here:

```text
vst-plugins/_installed/VST3
```

Stratum DAW scans that local folder when you use the channel rack `+` menu and choose re-scan/default scan.

## Add Another Plugin

Copy `StratumPiano` to a new folder, change the target name, `PRODUCT_NAME`, `PLUGIN_CODE`, and C++ class names.
