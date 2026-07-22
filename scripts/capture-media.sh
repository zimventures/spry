#!/usr/bin/env bash
# Regenerate the docs' WASM-demo fallback stills in one command (#11, #40).
#
# Builds the headless capture tool and renders each demo scene from
# examples/web/scenes.h to docs/assets/wasm/scene-*.png via an offscreen SDL
# software renderer — no display needed. These committed stills are the poster /
# no-JS fallback for the live demos; the site build just consumes them.
#
# Requires: a C++ toolchain + CMake (to build spry_capture).
# Usage:  scripts/capture-media.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

BUILD="${BUILD_DIR:-build-capture}"
ASSETS="docs/assets"

echo "==> Building spry_capture"
cmake -B "$BUILD" -DSPRY_BUILD_CAPTURE=ON -DSPRY_BUILD_DEMO=OFF -DSPRY_BUILD_TESTS=OFF >/dev/null
cmake --build "$BUILD" --config Release --target spry_capture

# Locate the binary — single-config generators put it in $BUILD, multi-config
# ones (Visual Studio / Xcode) under $BUILD/<Config>/; also handle the .exe suffix.
CAPTURE="$(find "$BUILD" -type f \( -name spry_capture -o -name spry_capture.exe \) 2>/dev/null | head -n1)"
if [ -z "$CAPTURE" ]; then
  echo "!! spry_capture binary not found under $BUILD" >&2
  exit 1
fi

echo "==> Rendering fallback stills ($CAPTURE)"
"$CAPTURE" "$ASSETS"

echo "==> Done. Stills written to $ASSETS/wasm/"
