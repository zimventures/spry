#!/usr/bin/env bash
# Fetch a pinned copy of doxygen-awesome-css into external/ so the Doxyfile can
# reference its stylesheet. Idempotent: re-running checks out the pinned tag.
#
# Usage:  scripts/fetch-doxygen-awesome.sh
set -euo pipefail

VERSION="v2.3.4"
DEST="external/doxygen-awesome-css"
REPO="https://github.com/jothepro/doxygen-awesome-css.git"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [ -d "$DEST/.git" ]; then
  echo "doxygen-awesome-css present; checking out $VERSION"
  git -C "$DEST" fetch --tags --quiet
  git -C "$DEST" checkout --quiet "$VERSION"
else
  echo "Cloning doxygen-awesome-css $VERSION into $DEST"
  git clone --quiet --depth 1 --branch "$VERSION" "$REPO" "$DEST"
fi

echo "Done: $DEST ($VERSION)"
