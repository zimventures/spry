#!/usr/bin/env bash
# Build the Spry WASM demo and stage it for the docs site (exploration / #wasm).
#
# Compiles examples/web/web_demo.cpp (SdlRenderer backend) to WebAssembly via
# Emscripten, embedding the font + themes, and copies the output next to the host
# page at docs/assets/wasm/. The docs build just serves the committed artifacts.
#
# Requires the Emscripten SDK (https://emscripten.org). Point $EMSDK at it, or it
# defaults to ~/emsdk.  Usage:  scripts/build-web.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# shellcheck disable=SC1091
source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1 || {
  echo "!! Emscripten not found. Install it and set \$EMSDK (see https://emscripten.org)." >&2
  exit 1
}

BUILD="${BUILD_DIR:-build-web}"
OUT="docs/assets/wasm"

echo "==> Configuring (Emscripten)"
emcmake cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
  -DSPRY_BUILD_DEMO=OFF -DSPRY_BUILD_TESTS=OFF >/dev/null

echo "==> Building spry_demos"
cmake --build "$BUILD" --target spry_demos

echo "==> Staging into $OUT/"
mkdir -p "$OUT"
cp "$BUILD/spry_demos.js" "$BUILD/spry_demos.wasm" "$OUT/"

echo "==> Done: $OUT/spry_demos.{js,wasm}  (host page: $OUT/demo.html?scene=<id>)"
