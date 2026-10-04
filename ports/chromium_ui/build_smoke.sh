#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium"
  exit 2
fi

CHROMIUM="$(cd "$1" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
KESH_ROOT="$(cd "$HERE/../.." && pwd)"

"$HERE/prepare_build.sh" "$CHROMIUM" --gen

TARGET="ozone_demo"
if command -v autoninja >/dev/null 2>&1; then
  (cd "$CHROMIUM" && autoninja -C out/KeshOS "$TARGET")
elif command -v ninja >/dev/null 2>&1; then
  ninja -C "$CHROMIUM/out/KeshOS" "$TARGET"
else
  echo "Neither autoninja nor ninja was found."
  exit 3
fi

BIN="$CHROMIUM/out/KeshOS/ozone_demo"
if [[ ! -f "$BIN" ]]; then
  echo "Build completed but $BIN was not found."
  exit 4
fi

mkdir -p "$KESH_ROOT/ports_bin"
cp -f "$BIN" "$KESH_ROOT/ports_bin/ozone_demo.elf"
chmod +x "$KESH_ROOT/ports_bin/ozone_demo.elf"

echo
echo "Staged: $KESH_ROOT/ports_bin/ozone_demo.elf"
echo "KeshOS ISO packaging already copies ports_bin/*.elf to /boot/apps/."
echo "First runtime args: --disable-gpu --ozone-platform=kesh"
