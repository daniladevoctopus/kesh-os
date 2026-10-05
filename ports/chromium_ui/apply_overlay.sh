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

# Chromium's embedder override enables PDFium's Rust PNG codec globally.
# OzoneKesh deliberately disables the Rust toolchain for the first C++/Skia
# milestone, and GN evaluates PDFium assertions even though kesh_smoke does
# not depend on PDFium. Keep the pinned checkout internally consistent.
PDFIUM_OVERRIDE="$SRC/build_overrides/pdfium.gni"
if [[ -f "$PDFIUM_OVERRIDE" ]]; then
  sed -i 's/^pdf_enable_rust_png_override = true$/pdf_enable_rust_png_override = false/' \
    "$PDFIUM_OVERRIDE"
fi
if ! grep -q '^pdf_enable_rust_png_override = false$' "$PDFIUM_OVERRIDE"; then
  echo "failed to disable PDFium Rust PNG override: $PDFIUM_OVERRIDE" >&2
  exit 4
fi

# Chromium 144 declares two Rust-backed //base targets even when an embedder
# explicitly disables the target Rust toolchain. The KeshOS milestone is a
# static musl C++/Skia executable, so apply the small pinned-source patch that
# makes those declarations and dependencies follow enable_rust. Keep this
# idempotent because tomorrow_build.sh is expected to be rerun incrementally.
for NO_RUST_PATCH in \
  "$HERE/patches/chromium-base-no-rust.patch" \
  "$HERE/patches/chromium-base-test-no-rust.patch" \
  "$HERE/patches/chromium-content-browser-stub.patch" \
  "$HERE/patches/chromium-webnn-no-tests.patch" \
  "$HERE/patches/chromium-viz-service-minimal.patch" \
  "$HERE/patches/chromium-mojo-no-rust.patch" \
  "$HERE/patches/chromium-skia-no-rust.patch" \
  "$HERE/patches/chromium-fontconfig-no-rust.patch" \
  "$HERE/patches/chromium-ui-base-minimal.patch" \
  "$HERE/patches/chromium-ui-gfx-minimal.patch" \
  "$HERE/patches/chromium-ui-resources-minimal.patch" \
  "$HERE/patches/chromium-ozone-no-tests.patch"; do
  if git -C "$SRC" apply --reverse --check "$NO_RUST_PATCH" >/dev/null 2>&1; then
    echo "No-Rust patch already applied: $(basename "$NO_RUST_PATCH")"
  elif git -C "$SRC" apply --check "$NO_RUST_PATCH" >/dev/null 2>&1; then
    git -C "$SRC" apply "$NO_RUST_PATCH"
    echo "Applied no-Rust patch: $(basename "$NO_RUST_PATCH")"
  else
    echo "failed to apply pinned Chromium no-Rust patch: $NO_RUST_PATCH" >&2
    exit 5
  fi
done

echo "OzoneKesh overlay installed at: $SRC/keshos"
echo
echo "GN bootstrap args:"
cat "$HERE/args.gn.example"
