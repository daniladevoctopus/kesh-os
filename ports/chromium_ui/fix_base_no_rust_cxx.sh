#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
FILE="$SRC/base/BUILD.gn"

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS no-Rust CXX bridge guard"

if marker in text:
    print("Base CXX bridge already guarded for no-Rust KeshOS")
    raise SystemExit(0)

old = '''  sources += [
    "containers/span_rust.h",
    "strings/string_view_rust.h",
  ]

  # Base provides conversions between CXX types and base types (e.g.
  # std::string_view).
  public_deps += [ "//build/rust:cxx_cppdeps" ]
'''

new = '''  # KeshOS no-Rust CXX bridge guard
  # These headers and the CXX support library only exist when Chromium's
  # Rust/CXX bridge is enabled. The first OzoneKesh milestone is C++ only.
  if (enable_rust_cxx) {
    sources += [
      "containers/span_rust.h",
      "strings/string_view_rust.h",
    ]

    # Base provides conversions between CXX types and base types (e.g.
    # std::string_view).
    public_deps += [ "//build/rust:cxx_cppdeps" ]
  }
'''

if old not in text:
    raise SystemExit("expected base Rust/CXX source+dependency block not found")

path.write_text(text.replace(old, new, 1))
print("Gated //base Rust/CXX bridge behind enable_rust_cxx")
PY
