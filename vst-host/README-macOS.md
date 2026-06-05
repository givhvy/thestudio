# Stratum DAW macOS Build

This project can be built as a native JUCE `.app` on macOS and packaged as a single `.dmg` installer.

## One-file installer

The repo includes a GitHub Actions workflow at:

```text
.github/workflows/macos-dmg.yml
```

Run **Build macOS DMG** from GitHub Actions. When it finishes, download the `Stratum-DAW-macOS-DMG` artifact. It contains:

```text
Stratum DAW.dmg
```

Open the DMG on the MacBook, drag `Stratum DAW.app` to `Applications`, then run it natively.

## App update checks

The DAW checks GitHub Releases for newer builds. To publish an update:

```sh
git tag v1.0.1
git push origin v1.0.1
```

The macOS workflow will build `Stratum DAW.dmg` and attach it to the GitHub Release for that tag. Installed apps will show `UPDATE!` when the release tag is newer than the app version.

## Requirements

- macOS 12 or newer
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`

## Build

From the `vst-host` folder on the MacBook:

```sh
npm --prefix ../frontend ci
npm --prefix ../frontend run build
chmod +x scripts/build-macos.sh scripts/package-macos-dmg.sh
./scripts/build-macos.sh
```

The app bundle will be created at:

```text
build-macos/StratumVSTHost_artefacts/Release/Stratum DAW.app
```

## Package a DMG

```sh
./scripts/package-macos-dmg.sh
```

The installer image will be created at:

```text
dist-macos/Stratum DAW.dmg
```

## Notes

- macOS builds use JUCE's native WebKit browser backend instead of Windows WebView2.
- AU plugin hosting is enabled on macOS, alongside VST3 hosting.
- `frontend/dist` is bundled into the `.app` under `Contents/Resources/frontend/dist`.
- Exported projects, WAVs, stems, and backups use `~/Documents/stratumdaw` on macOS.
- Producer tags use `~/Documents/tags` on macOS.
- A DMG built this way is unsigned. For clean install without Gatekeeper warnings on other Macs, sign and notarize with an Apple Developer ID.
