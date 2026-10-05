#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
FILE="$SRC/third_party/libpng/visibility.gni"

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS no-Rust Skia libpng fallback"

if marker in text:
    print("Skia libpng fallback visibility already enabled for KeshOS")
    raise SystemExit(0)

anchor = '# Self-dependency (for internal targets defined in\n'
pos = text.find(anchor)
if pos < 0:
    raise SystemExit("libpng visibility anchor not found")

addition = (
    '# KeshOS no-Rust Skia libpng fallback\n'
    '# Chromium normally migrated Skia PNG decoding to Rust. OzoneKesh has\n'
    '# enable_rust=false for the first milestone, so the patched //skia:skia\n'
    '# target deliberately falls back to libpng and needs this one visibility.\n'
    'visibility += [ "//skia:skia" ]\n\n'
)

path.write_text(text[:pos] + addition + text[pos:])
print("Allowed //skia:skia to use libpng for the no-Rust KeshOS build")
PY
