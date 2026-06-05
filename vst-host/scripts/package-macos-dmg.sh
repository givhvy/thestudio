#!/usr/bin/env zsh
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-macos"
APP="$BUILD_DIR/StratumVSTHost_artefacts/Release/Stratum DAW.app"
DIST_DIR="$PROJECT_DIR/dist-macos"
DMG="$DIST_DIR/Stratum DAW.dmg"
STAGE="$DIST_DIR/dmg-stage"

if [[ ! -d "$APP" ]]; then
  echo "App bundle not found. Run scripts/build-macos.sh first."
  exit 1
fi

rm -rf "$DIST_DIR"
mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"

hdiutil create \
  -volname "Stratum DAW" \
  -srcfolder "$STAGE" \
  -ov \
  -format UDZO \
  "$DMG"

echo "Packaged: $DMG"
