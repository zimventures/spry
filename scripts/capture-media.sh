#!/usr/bin/env bash
# Regenerate all docs screenshots + GIFs in one command (#11).
#
# Builds the headless capture tool, renders the scenes to PNG (light + dark) via an
# offscreen SDL software renderer — no display needed — then assembles the GIF
# frame sequence with ffmpeg. The committed assets under docs/assets/ are the
# output; the site build just consumes them (it never runs this).
#
# Requires: a C++ toolchain + CMake (to build spry_capture) and ffmpeg (for GIFs).
# Usage:  scripts/capture-media.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

BUILD="${BUILD_DIR:-build-capture}"
ASSETS="docs/assets"
FRAMES="$ASSETS/_gifframes"

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

echo "==> Rendering screenshots ($CAPTURE)"
"$CAPTURE" "$ASSETS"

if [ -d "$FRAMES" ]; then
  if command -v ffmpeg >/dev/null 2>&1; then
    echo "==> Assembling GIF (theme-swap.gif)"
    ffmpeg -y -loglevel error -framerate 30 -i "$FRAMES/frame_%03d.png" \
      -vf "split[s0][s1];[s0]palettegen=stats_mode=diff[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3" \
      "$ASSETS/theme-swap.gif"
  else
    echo "!! ffmpeg not found — skipping GIF assembly (screenshots still updated)"
  fi
  rm -rf "$FRAMES"
fi

echo "==> Done. Media written to $ASSETS/"
