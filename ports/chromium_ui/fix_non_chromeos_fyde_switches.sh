#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
FILE="$SRC/fydeos/switches/BUILD.gn"

if [[ ! -f "$FILE" ]]; then
  echo "Fyde switches BUILD.gn not found: $FILE" >&2
  exit 3
fi

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS non-ChromeOS GN compatibility"

if marker in text:
    print("Fyde switches already KeshOS-compatible")
    raise SystemExit(0)

import_line = 'import("//build/config/chromeos/rules.gni")\n\n'
assert_line = 'assert(is_chromeos, "Non-Chrome-OS builds must not depend on //chromeos")\n\n'

if import_line not in text:
    raise SystemExit("expected ChromeOS rules import not found in Fyde switches")
if assert_line not in text:
    raise SystemExit("expected ChromeOS assertion not found in Fyde switches")

text = text.replace(import_line, marker + "\n", 1)
text = text.replace(assert_line, "", 1)
path.write_text(text)
print("Removed ChromeOS-only GN assertion from Fyde switches for KeshOS")
PY
