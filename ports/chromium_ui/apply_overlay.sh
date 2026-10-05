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

PDFIUM_OVERRIDE="$SRC/build_overrides/pdfium.gni"
if [[ -f "$PDFIUM_OVERRIDE" ]]; then
  sed -i 's/^pdf_enable_rust_png_override = true$/pdf_enable_rust_png_override = false/' \
    "$PDFIUM_OVERRIDE"
fi
if ! grep -q '^pdf_enable_rust_png_override = false$' "$PDFIUM_OVERRIDE"; then
  echo "failed to disable PDFium Rust PNG override: $PDFIUM_OVERRIDE" >&2
  exit 4
fi

for NO_RUST_PATCH in \
  "$HERE/patches/chromium-base-no-rust.patch" \
  "$HERE/patches/chromium-base-test-no-rust.patch" \
  "$HERE/patches/chromium-content-browser-stub.patch" \
  "$HERE/patches/chromium-webnn-no-tests.patch" \
  "$HERE/patches/chromium-viz-service-minimal.patch" \
  "$HERE/patches/chromium-mojo-no-rust.patch" \
  "$HERE/patches/chromium-mojom-no-blink-overrides.patch" \
  "$HERE/patches/chromium-mojom-prune-blink-variant.patch" \
  "$HERE/patches/chromium-google-apis-no-fyde-switches.patch" \
  "$HERE/patches/chromium-device-no-usb-tests.patch" \
  "$HERE/patches/chromium-lens-no-chrome.patch" \
  "$HERE/patches/chromium-skia-no-rust.patch" \
  "$HERE/patches/chromium-fontconfig-no-rust.patch" \
  "$HERE/patches/chromium-ui-base-minimal.patch" \
  "$HERE/patches/chromium-ui-gfx-minimal.patch" \
  "$HERE/patches/chromium-ui-resources-minimal.patch" \
  "$HERE/patches/chromium-ozone-no-tests.patch"; do
  if git -C "$SRC" apply --reverse --check "$NO_RUST_PATCH" >/dev/null 2>&1; then
    echo "Pinned patch already applied: $(basename "$NO_RUST_PATCH")"
  elif git -C "$SRC" apply --check "$NO_RUST_PATCH" >/dev/null 2>&1; then
    git -C "$SRC" apply "$NO_RUST_PATCH"
    echo "Applied pinned patch: $(basename "$NO_RUST_PATCH")"
  else
    echo "failed to apply pinned Chromium patch: $NO_RUST_PATCH" >&2
    exit 5
  fi
done

# Persistent checkout may carry previous patch generations, so use semantic
# edits for tiny test-only edges whose surrounding context changes often.
python3 - "$SRC/services/device/BUILD.gn" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
start = text.find('source_set("usb_test_gadget")')
if start < 0:
    raise SystemExit("usb_test_gadget target not found")
end = text.find('\nsource_set(', start + 1)
if end < 0:
    end = len(text)
block = text[start:end]
needle = '    "//services/device/usb",\n'
if needle in block:
    block = block.replace(needle, '', 1)
    text = text[:start] + block + text[end:]
    path.write_text(text)
    print("Pruned Blink-only //services/device/usb from usb_test_gadget")
else:
    print("usb_test_gadget already has no Blink USB dependency")
PY

python3 - "$SRC/third_party/blink/public/BUILD.gn" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()

def gate_list(text, marker, variable):
    start = text.find(marker)
    if start < 0:
        raise SystemExit(f"target not found: {marker}")
    end = text.find('\n}\n', start)
    if end < 0:
        raise SystemExit(f"target end not found: {marker}")
    block = text[start:end + 3]
    gated = f'  {variable} = []\n  if (use_blink) {{'
    if gated in block:
        return text, False
    list_start = block.find(f'  {variable} = [')
    if list_start < 0:
        raise SystemExit(f"{variable} list not found in {marker}")
    list_end = block.find('\n  ]', list_start)
    if list_end < 0:
        raise SystemExit(f"{variable} list end not found in {marker}")
    list_end += len('\n  ]')
    original = block[list_start:list_end]
    lines = original.splitlines()
    body = '\n'.join('  ' + line for line in lines)
    replacement = f'  {variable} = []\n  if (use_blink) {{\n{body}\n  }}'
    block = block[:list_start] + replacement + block[list_end:]
    return text[:start] + block + text[end + 3:], True

changed = False
for marker, variable in [
    ('group("blink")', 'deps'),
    ('group("test_support")', 'public_deps'),
    ('group("all_blink")', 'public_deps'),
    ('source_set("blink_headers")', 'public_deps'),
    ('source_set("blink_headers")', 'deps'),
]:
    text, did = gate_list(text, marker, variable)
    changed = changed or did
if changed:
    path.write_text(text)
    print("Gated Blink renderer/public dependency lists behind use_blink")
else:
    print("Blink renderer/public dependency lists already gated")
PY

# Content Shell has no valid role in a build that explicitly disables Blink.
# Its BUILD file may still be loaded by unrelated Chromium test/tooling edges;
# keep declarations/args visible, but do not evaluate shell targets themselves.
python3 - "$SRC/content/shell/BUILD.gn" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS no-Blink Content Shell guard"
if marker not in text:
    anchor = 'config("content_shell_lib_warnings") {'
    pos = text.find(anchor)
    if pos < 0:
        raise SystemExit("content_shell_lib_warnings anchor not found")
    prefix = text[:pos]
    body = text[pos:].rstrip() + "\n"
    guarded = (
        marker + "\n"
        "if (use_blink) {\n" +
        "\n".join("  " + line if line else "" for line in body.splitlines()) +
        "\n}\n"
    )
    path.write_text(prefix + guarded)
    print("Guarded Content Shell targets behind use_blink")
else:
    print("Content Shell targets already guarded")
PY

echo "OzoneKesh overlay installed at: $SRC/keshos"
echo
echo "GN bootstrap args:"
cat "$HERE/args.gn.example"
