#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium"
  exit 2
fi

CHROMIUM="$(cd "$1" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
KESH_ROOT="$(cd "$HERE/../.." && pwd)"
JOBS="${KESH_CHROMIUM_JOBS:-2}"

"$HERE/prepare_build.sh" "$CHROMIUM" --gen

# Build our deliberately tiny first milestone instead of Chromium's upstream
# ozone_demo. The upstream demo also links GL, tracing and Mojo setup, none of
# which are needed to prove Skia -> OzoneKesh -> KeshOS framebuffer.
TARGET="keshos/ozone:kesh_smoke"
if command -v autoninja >/dev/null 2>&1; then
  (cd "$CHROMIUM" && NINJA_SUMMARIZE_BUILD=1 autoninja -j "$JOBS" -C out/KeshOS "$TARGET")
elif command -v ninja >/dev/null 2>&1; then
  ninja -j "$JOBS" -C "$CHROMIUM/out/KeshOS" "$TARGET"
else
  echo "Neither autoninja nor ninja was found."
  exit 3
fi

BIN="$CHROMIUM/out/KeshOS/kesh_smoke"
if [[ ! -f "$BIN" ]]; then
  BIN="$(find "$CHROMIUM/out/KeshOS" -type f -name kesh_smoke -print -quit)"
fi
if [[ -z "${BIN:-}" || ! -f "$BIN" ]]; then
  echo "Build completed but the kesh_smoke executable was not found."
  exit 4
fi

READELF="$(command -v llvm-readelf || command -v readelf || true)"
if [[ -z "$READELF" ]]; then
  echo "llvm-readelf/readelf is required to validate the KeshOS binary."
  exit 5
fi

HEADER="$($READELF -h "$BIN")"
PROGRAMS="$($READELF -l "$BIN")"

if ! grep -Eq 'Type:[[:space:]]+EXEC' <<<"$HEADER"; then
  echo "ERROR: kesh_smoke is not ET_EXEC. KeshOS loader rejects PIE/ET_DYN."
  echo "$HEADER"
  exit 10
fi

if grep -Eq 'INTERP|DYNAMIC' <<<"$PROGRAMS"; then
  echo "ERROR: kesh_smoke contains PT_INTERP/PT_DYNAMIC. KeshOS needs a static executable."
  echo "$PROGRAMS"
  exit 11
fi

# KeshOS enforces W^X in the ELF loader. Accept normal R E and RW segments,
# reject any LOAD segment whose flag column contains RWE.
if "$READELF" -W -l "$BIN" | grep 'LOAD' | grep -Eq 'RWE|RW E|R E W'; then
  echo "ERROR: kesh_smoke contains a writable+executable LOAD segment."
  "$READELF" -W -l "$BIN" | grep LOAD
  exit 12
fi

STAGED="$KESH_ROOT/ports_bin/kesh_smoke.elf"
mkdir -p "$KESH_ROOT/ports_bin"
cp -f "$BIN" "$STAGED"

# Strip only the staged copy. Keep Chromium's original output intact for
# debugging compiler/linker failures.
STRIP="$(command -v llvm-strip || command -v strip || true)"
if [[ -n "$STRIP" ]]; then
  "$STRIP" --strip-debug "$STAGED" || true
fi
chmod +x "$STAGED"

SIZE="$(du -h "$STAGED" | awk '{print $1}')"
echo
echo "=============================================="
echo " OzoneKesh smoke binary is ready"
echo "=============================================="
echo "Staged: $STAGED ($SIZE)"
echo "ELF: static ET_EXEC, W^X validation passed"
echo "Runtime Ozone default: kesh"
echo "KeshOS ISO packaging copies ports_bin/*.elf to /boot/apps/."
echo
echo "Next: ./build.sh"
