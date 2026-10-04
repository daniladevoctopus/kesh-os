#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium"
  exit 2
fi

SRC="$(cd "$1" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
PIN="493c46885be032faa677b4c166e06c7e85a0c396"

if [[ ! -f "$SRC/ui/ozone/BUILD.gn" || ! -f "$SRC/build/config/ozone.gni" ]]; then
  echo "not a Chromium/OpenFyde source root: $SRC"
  exit 3
fi

if [[ -d "$SRC/.git" ]]; then
  HEAD="$(git -C "$SRC" rev-parse HEAD)"
  if [[ "$HEAD" != "$PIN" ]]; then
    echo "WARNING: expected OpenFyde commit $PIN"
    echo "         checkout is currently       $HEAD"
  fi
fi

rm -rf "$SRC/keshos"
cp -a "$HERE/chromium_overlay/keshos" "$SRC/keshos"

echo "OzoneKesh overlay installed at: $SRC/keshos"
echo
echo "GN bootstrap args:"
cat "$HERE/args.gn.example"
