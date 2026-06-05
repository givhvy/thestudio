#!/usr/bin/env zsh
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-macos"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required. Install it with: brew install cmake"
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools are required. Install them with: xcode-select --install"
  exit 1
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Xcode \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}" \
  -DCMAKE_OSX_ARCHITECTURES="${MACOS_ARCHS:-arm64;x86_64}"

cmake --build "$BUILD_DIR" --config Release

APP="$BUILD_DIR/StratumVSTHost_artefacts/Release/Stratum DAW.app"
if [[ ! -d "$APP" ]]; then
  echo "Build completed, but app bundle was not found at: $APP"
  exit 1
fi

echo "Built: $APP"
