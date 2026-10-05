#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
FILE="$SRC/content/BUILD.gn"

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS no-Blink GN compatibility: public/common may be parsed by UI deps."
old = 'assert(use_blink, "Chromium without blink shouldn\'t use anything in //content")\n'
if marker in text:
    print("Content no-Blink assertion already relaxed")
elif old in text:
    text = text.replace(old, marker + "\n", 1)
    path.write_text(text)
    print("Relaxed //content top-level Blink assertion for KeshOS GN generation")
else:
    raise SystemExit("expected //content use_blink assertion not found")
PY
