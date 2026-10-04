#!/usr/bin/env bash
set -euo pipefail

# Fetch the exact OpenFyde Chromium tree used by OzoneKesh. Chromium's own
# README recommends depot_tools/gclient rather than a standalone git clone,
# because DEPS supplies Skia, buildtools and the other coupled repositories.

PIN="493c46885be032faa677b4c166e06c7e85a0c396"
OPENFYDE_URL="https://github.com/openFyde/chromium.git"

HERE="$(cd "$(dirname "$0")" && pwd)"
KESH_ROOT="$(cd "$HERE/../.." && pwd)"
WORKSPACE="${1:-$KESH_ROOT/../openfyde-kesh-work}"
DEPOT_TOOLS="${DEPOT_TOOLS:-$WORKSPACE/depot_tools}"
SRC="$WORKSPACE/src"

mkdir -p "$WORKSPACE"
WORKSPACE="$(cd "$WORKSPACE" && pwd)"
DEPOT_TOOLS="${DEPOT_TOOLS/#\~/$HOME}"
SRC="$WORKSPACE/src"

if [[ ! -d "$DEPOT_TOOLS/.git" ]]; then
  echo "[1/4] Fetching depot_tools..."
  git clone --depth 1 \
    https://chromium.googlesource.com/chromium/tools/depot_tools.git \
    "$DEPOT_TOOLS"
else
  echo "[1/4] depot_tools already present: $DEPOT_TOOLS"
fi

export PATH="$DEPOT_TOOLS:$PATH"
export DEPOT_TOOLS_UPDATE=0

command -v gclient >/dev/null 2>&1 || {
  echo "gclient is unavailable even after adding depot_tools to PATH" >&2
  exit 2
}

cd "$WORKSPACE"
if [[ ! -f .gclient ]]; then
  echo "[2/4] Configuring OpenFyde Chromium solution..."
  gclient config --name src "$OPENFYDE_URL"
else
  echo "[2/4] Existing .gclient found. Reusing it."
fi

# Keep the checkout pinned. --no-history prevents years of Chromium history
# from eating disk space. We intentionally let gclient fetch DEPS because the
# Ozone/Skia/base build cannot be produced from the chromium Git repo alone.
echo "[3/4] Syncing pinned OpenFyde revision $PIN ..."
gclient sync \
  --revision "src@$PIN" \
  --no-history \
  --nohooks \
  --delete_unversioned_trees=false

if [[ ! -d "$SRC/.git" ]]; then
  echo "OpenFyde source was not created at $SRC" >&2
  exit 3
fi

ACTUAL="$(git -C "$SRC" rev-parse HEAD)"
if [[ "$ACTUAL" != "$PIN" ]]; then
  echo "Pinned revision mismatch: expected $PIN, got $ACTUAL" >&2
  exit 4
fi

echo "[4/4] Running Chromium DEPS hooks..."
gclient runhooks

echo
echo "=============================================="
echo " OpenFyde source is ready"
echo "=============================================="
echo "Source: $SRC"
echo "Commit: $ACTUAL"
echo
echo "Next command:"
echo "  $KESH_ROOT/ports/chromium_ui/build_smoke.sh '$SRC'"
