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

# OzoneKesh now carries the tiny framebuffer/input ABI definitions it needs,
# so the KeshOS sysroot does not need Linux kernel UAPI header packages.
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

for tool in clang clang++ llvm-ar llvm-nm llvm-readelf; do
  if [[ ! -x "$CLANG_BIN/$tool" ]] && ! command -v "$tool" >/dev/null 2>&1; then
    echo "required LLVM tool not found: $tool"
    exit 41
  fi
done

# The KeshOS checkout path may contain spaces/Cyrillic. Give Ninja a stable
# no-space sysroot path.
ln -sfn "$SYSROOT_REAL" "$SYSROOT_LINK"

"$HERE/apply_overlay.sh" "$CHROMIUM"

mkdir -p "$CHROMIUM/out/KeshOS"
cat > "$CHROMIUM/out/KeshOS/args.gn" <<EOF
target_os = "linux"
target_cpu = "x64"

is_keshos_build = true
root_extra_deps = [ "//keshos/ozone:kesh_smoke" ]

custom_toolchain = "//keshos/toolchain:kesh_x64"
keshos_sysroot = "$SYSROOT_LINK"
keshos_clang_bin = "$CLANG_BIN"
use_sysroot = false
use_blink = false
use_glib = false
use_dbus = false
use_gtk = false
use_qt = false
media_use_symphonia = false
libyuv_disable_jpeg = true

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

# First milestone: software Skia only. Keep the binary small enough for the
# current KeshOS loader and avoid GPU/driver dependencies until Ozone works.
enable_vulkan = false
is_component_build = false
is_debug = false
is_official_build = true
chrome_pgo_phase = 0
symbol_level = 0
treat_warnings_as_errors = false
use_custom_libcxx = true

# Chromium's normal Linux Rust target is GNU. Ozone demo does not need Rust and
# keeping it disabled removes another cross-toolchain variable for bring-up.
enable_rust = false
enable_rust_cxx = false
EOF

echo "Prepared: $CHROMIUM/out/KeshOS/args.gn"
echo "KeshOS sysroot: $SYSROOT_LINK -> $SYSROOT_REAL"
echo "Clang tools: $CLANG_BIN"

echo "OpenFyde source HEAD: $(git -C "$CHROMIUM" rev-parse HEAD 2>/dev/null || echo unknown)"

if [[ "$DO_GEN" == "--gen" ]]; then
  GN=""
  if [[ -x "$CHROMIUM/buildtools/linux64/gn" ]]; then
    GN="$CHROMIUM/buildtools/linux64/gn"
  elif command -v gn >/dev/null 2>&1; then
    GN="$(command -v gn)"
  else
    echo "GN was not found. Chromium DEPS/buildtools are incomplete."
    echo "Run ports/chromium_ui/bootstrap_openfyde.sh first."
    exit 5
  fi

  (cd "$CHROMIUM" && "$GN" gen out/KeshOS)
fi
