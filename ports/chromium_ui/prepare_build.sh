#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium [--gen]"
  exit 2
fi

CHROMIUM="$(cd "$1" && pwd)"
DO_GEN="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
KESH_ROOT="$(cd "$HERE/../.." && pwd)"
SYSROOT_REAL="$KESH_ROOT/toolchain/musl"
SYSROOT_LINK="/tmp/keshos-chromium-sysroot"

if [[ ! -d "$SYSROOT_REAL/include" || ! -d "$SYSROOT_REAL/lib" ]]; then
  echo "KeshOS musl sysroot is missing: $SYSROOT_REAL"
  echo "Build/install the KeshOS musl toolchain first."
  exit 3
fi

# OzoneKesh M1/M2 intentionally consumes the Linux-compatible ABI headers that
# KeshOS already implements in its own kernel. Fail before a multi-hour
# Chromium build if the sysroot is incomplete.
for required in \
  "$SYSROOT_REAL/include/linux/fb.h" \
  "$SYSROOT_REAL/include/linux/input.h"; do
  if [[ ! -f "$required" ]]; then
    echo "KeshOS sysroot is missing required compatibility header: $required"
    exit 31
  fi
done

if [[ ! -e "$SYSROOT_REAL/lib/libc.a" ]]; then
  echo "KeshOS sysroot is missing static musl libc: $SYSROOT_REAL/lib/libc.a"
  exit 32
fi

CLANG="$(command -v clang || true)"
if [[ -z "$CLANG" ]]; then
  echo "clang was not found in PATH"
  exit 4
fi
CLANG_BIN="$(dirname "$(readlink -f "$CLANG")")"

# The KeshOS checkout path may contain spaces/Cyrillic. Give Ninja a stable
# no-space path.
ln -sfn "$SYSROOT_REAL" "$SYSROOT_LINK"

"$HERE/apply_overlay.sh" "$CHROMIUM"

mkdir -p "$CHROMIUM/out/KeshOS"
cat > "$CHROMIUM/out/KeshOS/args.gn" <<EOF
target_os = "linux"
target_cpu = "x64"

custom_toolchain = "//keshos/toolchain:kesh_x64"
keshos_sysroot = "$SYSROOT_LINK"
keshos_clang_bin = "$CLANG_BIN"
use_sysroot = false

use_ozone = true
ozone_auto_platforms = false
ozone_extra_path = "//keshos/ozone_extra.gni"
ozone_platform = "kesh"
ozone_platform_headless = false
ozone_platform_drm = false
ozone_platform_wayland = false
ozone_platform_x11 = false
ozone_platform_cast = false
ozone_platform_flatland = false

enable_vulkan = false
is_component_build = false
is_debug = true
symbol_level = 1
treat_warnings_as_errors = false

enable_rust = false
enable_rust_cxx = false
EOF

echo "Prepared: $CHROMIUM/out/KeshOS/args.gn"
echo "KeshOS sysroot: $SYSROOT_LINK -> $SYSROOT_REAL"
echo "Clang tools: $CLANG_BIN"

if [[ "$DO_GEN" == "--gen" ]]; then
  GN=""
  if [[ -x "$CHROMIUM/buildtools/linux64/gn" ]]; then
    GN="$CHROMIUM/buildtools/linux64/gn"
  elif command -v gn >/dev/null 2>&1; then
    GN="$(command -v gn)"
  else
    echo "GN was not found. Prepare Chromium DEPS/buildtools first."
    exit 5
  fi

  (cd "$CHROMIUM" && "$GN" gen out/KeshOS)
fi
