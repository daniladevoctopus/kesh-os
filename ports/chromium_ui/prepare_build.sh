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

ln -sfn "$SYSROOT_REAL" "$SYSROOT_LINK"

"$HERE/apply_overlay.sh" "$CHROMIUM"
bash "$HERE/fix_non_chromeos_fyde_switches.sh" "$CHROMIUM"
bash "$HERE/fix_no_blink_content.sh" "$CHROMIUM"
bash "$HERE/fix_base_no_rust_cxx.sh" "$CHROMIUM"
bash "$HERE/fix_skia_libpng_visibility.sh" "$CHROMIUM"
bash "$HERE/fix_partitionalloc_musl_cdefs.sh" "$CHROMIUM"

# Chromium's Linux resolver wrapper assumes glibc's re-entrant res_ninit /
# res_nclose APIs. musl intentionally exposes only the global resolver state
# through res_init() + _res. Treat non-glibc Linux like Chromium's existing
# OpenBSD/Fuchsia path so the smoke build stays inside the KeshOS musl ABI.
python3 - "$CHROMIUM/net/dns/public/scoped_res_state.cc" \
          "$CHROMIUM/net/dns/public/scoped_res_state.h" <<'PY'
from pathlib import Path
import sys

cc = Path(sys.argv[1])
h = Path(sys.argv[2])

positive_old = "#if BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA)"
positive_new = "#if BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA) || \\\n    (BUILDFLAG(IS_LINUX) && !defined(__GLIBC__))"
negative_old = "#if !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA)"
negative_new = "#if !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA) && \\\n    !(BUILDFLAG(IS_LINUX) && !defined(__GLIBC__))"

cc_text = cc.read_text()
if positive_new not in cc_text:
    if cc_text.count(positive_old) != 2:
        raise SystemExit(
            f"expected 2 positive resolver guards, found {cc_text.count(positive_old)}"
        )
    cc_text = cc_text.replace(positive_old, positive_new)
if negative_new not in cc_text:
    if cc_text.count(negative_old) != 1:
        raise SystemExit(
            f"expected 1 negative resolver guard, found {cc_text.count(negative_old)}"
        )
    cc_text = cc_text.replace(negative_old, negative_new, 1)
cc.write_text(cc_text)
print("Chromium resolver source adapted for musl res_init/_res")

h_text = h.read_text()
if negative_new not in h_text:
    if h_text.count(negative_old) != 1:
        raise SystemExit(
            f"expected 1 resolver header guard, found {h_text.count(negative_old)}"
        )
    h_text = h_text.replace(negative_old, negative_new, 1)
    h.write_text(h_text)
print("Chromium resolver header adapted for musl global state")
PY

mkdir -p "$CHROMIUM/out/KeshOS"
cat > "$CHROMIUM/out/KeshOS/args.gn" <<EOF
target_os = "linux"
target_cpu = "x64"

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

# HTTP Negotiate/Kerberos pulls the host GSSAPI stack into Chromium's generic
# Linux build. KeshOS does not expose Kerberos/GSSAPI yet and the smoke target
# does not need enterprise HTTP auth, so keep that optional feature out of the
# first native UI bring-up rather than leaking host headers into the musl target.
use_kerberos = false

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
is_debug = false
is_official_build = true
chrome_pgo_phase = 0
symbol_level = 0
treat_warnings_as_errors = false
use_custom_libcxx = true

# The KeshOS target uses the host distro's clang driver. Chromium's patched
# clang-only plugins (find-bad-constructs/raw-ptr/unsafe-buffers) are not
# present there, so keep them off for the cross target.
clang_use_chrome_plugins = false
clang_use_raw_ptr_plugin = false
clang_use_unsafe_buffers_plugin = false
use_clang_modules = false

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

  # Restrict generation to the smoke target and its transitive closure. This
  # keeps browser, Ash and unrelated test targets outside the first milestone.
  (cd "$CHROMIUM" && "$GN" gen out/KeshOS \
    --root-target=//keshos/ozone:kesh_smoke \
    --root-pattern=//keshos/ozone:kesh_smoke)
fi
