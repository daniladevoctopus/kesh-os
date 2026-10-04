#!/usr/bin/env bash
set -euo pipefail

# One-command bring-up for the first OpenFyde/Chromium UI code running on
# KeshOS. Run from any directory:
#   ./ports/chromium_ui/tomorrow_build.sh
#
# Optional:
#   OPENFYDE_WORKSPACE=/fast/disk/openfyde-kesh-work \
#   KESH_CHROMIUM_JOBS=2 ./ports/chromium_ui/tomorrow_build.sh

HERE="$(cd "$(dirname "$0")" && pwd)"
KESH_ROOT="$(cd "$HERE/../.." && pwd)"
WORKSPACE="${OPENFYDE_WORKSPACE:-$KESH_ROOT/../openfyde-kesh-work}"
SRC="$WORKSPACE/src"
PIN="493c46885be032faa677b4c166e06c7e85a0c396"

cd "$KESH_ROOT"

echo "=============================================="
echo " KeshOS + OpenFyde OzoneKesh bring-up"
echo "=============================================="
echo "KeshOS:   $KESH_ROOT"
echo "OpenFyde: $SRC"
echo "Jobs:     ${KESH_CHROMIUM_JOBS:-2}"
echo

missing=()
for tool in git python3 ninja clang clang++ cmake xorriso; do
  command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if (( ${#missing[@]} != 0 )); then
  echo "Missing host tools: ${missing[*]}"
  echo
  echo "On CachyOS/Arch install the normal build set with:"
  echo "  sudo pacman -S --needed base-devel git python ninja clang lld cmake xorriso"
  exit 2
fi

if [[ ! -f "$KESH_ROOT/toolchain/musl/lib/libc.a" ]]; then
  echo "KeshOS musl sysroot is missing: $KESH_ROOT/toolchain/musl"
  echo "Restore/build the existing KeshOS musl sysroot before Chromium."
  exit 3
fi

if [[ ! -d "$SRC/.git" ]]; then
  echo "[1/4] OpenFyde source is not present. Bootstrapping it now."
  "$HERE/bootstrap_openfyde.sh" "$WORKSPACE"
else
  echo "[1/4] Reusing OpenFyde checkout."
  actual="$(git -C "$SRC" rev-parse HEAD)"
  if [[ "$actual" != "$PIN" ]]; then
    echo "OpenFyde is at $actual but this port is pinned to $PIN."
    echo "Re-sync with: $HERE/bootstrap_openfyde.sh '$WORKSPACE'"
    exit 4
  fi
fi

# Keep target parallelism low by default so a 2-core/4-thread development
# machine remains usable during the first cross-build.
echo "[2/4] Building minimal Chromium/Skia OzoneKesh smoke app..."
KESH_CHROMIUM_JOBS="${KESH_CHROMIUM_JOBS:-2}" \
  "$HERE/build_smoke.sh" "$SRC"

echo "[3/4] Building KeshOS + bootable ISO..."
./build.sh

if [[ ! -f build/keshos.iso ]]; then
  echo "KeshOS build completed without build/keshos.iso" >&2
  exit 5
fi

if [[ -f tools/verify_iso.py ]]; then
  python3 tools/verify_iso.py
fi

echo "[4/4] Done."
echo
echo "=============================================="
echo " FIRST OZONEKESH IMAGE READY"
echo "=============================================="
echo "ISO: $KESH_ROOT/build/keshos.iso"
echo "App inside ISO: /boot/apps/kesh_smoke.elf"
echo
echo "Boot KeshOS, open Terminal and run:"
echo "  run /boot/apps/kesh_smoke.elf"
echo
echo "Expected picture: dark Chromium/Skia test shell with three cards,"
echo "an orange bar and a moving green heartbeat at the bottom."
echo
echo "Rendering path:"
echo "  Chromium base + Skia -> OzoneKesh -> /dev/fb0 -> KeshOS kernel"
echo
echo "The first smoke app intentionally runs until reboot/termination."
echo "If it fails, save serial.log plus the final compiler/runtime output."
