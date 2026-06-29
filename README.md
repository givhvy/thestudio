# Stratum DAW (macOS port)

Cross-platform JUCE music studio ported from Windows. Runs natively on macOS as a standalone .app, plus two instrument plugins (Stratum Piano / Stratum Guitar) compiled as AU and VST3.

- **Origin repo:** https://github.com/givhvy/thestudio
- **Upstream product:** Stratum DAW (FL Studio-style UI on a JUCE host, React frontend in WKWebView)
- **Apple tools:** Xcode 26.6 CLT, CMake 4.3.4, JUCE 8.0.14 (cloned at `vst-host/JUCE`)
- **Host binary:** `vst-host/build-macos/StratumVSTHost_artefacts/Stratum DAW.app` (≈ 44 MB)
- **Plugins:**
  - VST3: `vst-plugins/_installed/VST3/Stratum Piano.vst3`, `Stratum Guitar.vst3`
  - AU: auto-installed to `~/Library/Audio/Plug-Ins/Components/Stratum {Piano,Guitar}.component` by JUCE's `COPY_PLUGIN_AFTER_BUILD`

## What was ported from Windows

The upstream code targeted **JUCE 7.x** and the Windows-only Win32 API. It is now building on macOS 26 (Apple Silicon) against **JUCE 8.0.14** with the following code edits:

| File | Change | Reason |
|------|--------|--------|
| `vst-host/src/BottomDock.h` | `~BottomDock() override = default;` → declared only | `std::unique_ptr<SessionVideoHost>` (forward-declared nested class) needs the complete type to instantiate the implicit destructor; libc++ enforces this where MSVC was lenient |
| `vst-host/src/BottomDock.cpp` | Added `BottomDock::~BottomDock() = default;` | Out-of-line definition where `SessionVideoHost` is complete |
| `vst-host/src/PluginHost.cpp:703` | `createEditor()` → `createEditorIfNeeded()` | JUCE 8 made `AudioProcessor::createEditor()` private |
| `vst-host/src/Browser.cpp:1469` | `Font::getStringWidth(s)` → `GlyphArrangement::getStringWidthInt(font, s)` | Member function removed in JUCE 8 |
| `vst-host/src/MainComponent.cpp:62` | `Font::getStringWidthFloat(s)` → `GlyphArrangement::getStringWidth(font, s)` | Same |
| `vst-host/src/YouTubePanel.cpp:399` | `Font::getStringWidth(s)` → `GlyphArrangement::getStringWidthInt(font, s)` | Same |
| 7× `juce::File = {};` | Replaced with `juce::File()` | `{}` is ambiguous between `operator=(const File&)` and `operator=(const String&)` in JUCE 8 / libc++ |
| `vst-host/src/MainComponent.cpp:4489-4503` (Pinterest download) | Wrapped `SHELLEXECUTEINFOW / WaitForSingleObject / CloseHandle` block in `#if JUCE_WINDOWS`; added macOS branch using `juce::ChildProcess` + `python3` | Windows ShellExecute API was not guarded |
| `vst-host/src/MainComponent.cpp:4577-4585` (Pinterest login) | Same `#if JUCE_WINDOWS` guard; macOS branch spawns a `Terminal.app` window via `osascript` | Same |
| `vst-host/src/MainComponent.cpp` (top) | Added `#include <cstdlib>` | Needed for `std::system` in the macOS Pinterest-login branch |
| `vst-plugins/StratumPiano/CMakeLists.txt` | `FORMATS VST3` → `FORMATS AU VST3` | Enable AU build on macOS |
| `vst-plugins/StratumGuitar/CMakeLists.txt` | Same | Same |

No other source edits. All other Windows-specific code in `vst-host/src/*.cpp` is already correctly guarded by `#if JUCE_WINDOWS` / `#ifdef _WIN32` and compiles unchanged on macOS.

## Build (from scratch)

```sh
# one-time
brew install cmake
cd vst-host
git clone --depth 1 --branch 8.0.14 https://github.com/juce-framework/JUCE.git JUCE

# frontend (React) must be built first; the standalone .app bundles frontend/dist
cd ../frontend
npm install
npm run build
cd ../vst-host

# standalone host
cmake -S . -B build-macos -G "Unix Makefiles" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
cmake --build build-macos --config Release -j 8

# plugins
cd ../vst-plugins
cmake -S . -B build-macos -G "Unix Makefiles" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
cmake --build build-macos --config Release -j 8
```

> **Why `Unix Makefiles` and not `Xcode`?**
> This Mac's `xcode-select` is pinned to `/Library/Developer/CommandLineTools` (sudo is required to repoint to `/Applications/Xcode.app/Contents/Developer`). CMake's `Xcode` generator invokes `xcodebuild` internally during compiler probes and fails with "tool 'xcodebuild' requires Xcode, but active developer directory is a command line tools instance." `Unix Makefiles` skips that and just calls `clang`/`clang++` directly.

## Run

```sh
# ad-hoc sign (only needed once after build) and launch
codesign --force --deep --sign - "vst-host/build-macos/StratumVSTHost_artefacts/Stratum DAW.app"
open "vst-host/build-macos/StratumVSTHost_artefacts/Stratum DAW.app"
```

The standalone window is a full DAW: Channel Rack (TRACK 1–14), Pattern step-sequencer, Transport (BPM 130 default), Mixer Preview, Browser for plugins and drums, plus top-level menus for Theme, Pinterest, 808 MIDI, Consistency, DistroKid, Changelog, Backup, Update.

## Verify plugins

```sh
# AU install location
ls ~/Library/Audio/Plug-Ins/Components/ | grep Stratum
# VST3 install (manual — only the in-build VST3 dir is auto-populated)
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R vst-plugins/_installed/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/

# Apple validator (expected to FAIL — see notes below)
auval -v aumu Stra Pia1
auval -v aumu Stra Gtr1
```

## Known limitations

- **`auval` fails on ad-hoc-signed AUs** (`Error from retrieving Component Version: -50`). This is expected: AU host apps (Logic, Ableton, Reaper, JUCE's own scanner) load them fine via their own scan paths. For a clean `auval` pass, sign with a real Apple Developer ID and notarize.
- **No code signing / notarization.** Built `.app` and `.component`/`.vst3` bundles are ad-hoc signed only. Gatekeeper will warn on first open of a DMG-distributed build.
- **`drums folder not found`** message in the Browser panel is a runtime config: the upstream project references a Windows sample path. Edit `Browser.cpp` to point to a macOS drum library.
- **JUCE 7.0.12 (last 7.x) does not compile** on this Mac — `juceaide` calls `CGWindowListCreateImage` which is `unavailable` (not just deprecated) on macOS 15+. The code is targeted at JUCE 8 with the porting fixes above; downgrading is not viable without bigger changes.
- **`xcode-select`** is pinned to CLT (sudo required to change). If you ever run `sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer`, the official `scripts/build-macos.sh` (Xcode generator + DMG packager) will work end-to-end.

## Folder layout (relative to this file)

```
Mac/StratumDAW/
├── vst-host/                 # Standalone host (CMake + JUCE 8)
│   ├── JUCE/                 # vendored, branch 8.0.14
│   ├── src/                  # 85 .cpp/.h files (~3.2 MB)
│   ├── build-macos/          # CMake output → Stratum DAW.app
│   └── scripts/              # upstream build-macos.sh / package-macos-dmg.sh
├── vst-plugins/              # Stratum Piano + Stratum Guitar (CMake + JUCE 8)
│   ├── StratumPiano/         # AU + VST3
│   ├── StratumGuitar/        # AU + VST3
│   ├── _installed/VST3/      # CMake COPY_PLUGIN_AFTER_BUILD output
│   └── build-macos/          # CMake output
├── frontend/                 # React UI (vite build → frontend/dist → bundled into .app)
├── backend/                  # Python/Prisma server (not needed for the Mac native app)
├── electron/                 # Electron wrapper (not needed for the Mac native app)
├── screenshots/              # first-launch.png, after-plugins.png
├── .git/                     # sparse-checkout of givhvy/thestudio
└── README.md                 # this file
```

## Verification

- `Stratum DAW.app` launches and renders the full UI (see `screenshots/first-launch.png`).
- AU + VST3 bundles exist for both Stratum Piano and Stratum Guitar.
- `auval` is the only thing that fails; this is the ad-hoc signing limitation above.
