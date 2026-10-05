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
else:
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

# Chromium removed its C++ JSON parser after switching base::JSONReader to a
# Rust implementation. KeshOS intentionally disables Rust for the first native
# UI bring-up, so restore Chromium's own last C++ parser from the exact parent
# revision of that removal. Some gclient checkouts do not retain that historical
# blob locally, therefore fall back to the immutable raw GitHub object.
LEGACY_JSON_COMMIT="86f79d7237bd4d73273f51614d431976aff5e1c3"
for name in json_parser.h json_parser.cc; do
  dst="$SRC/base/json/$name"
  if [[ ! -s "$dst" ]]; then
    tmp="${dst}.kesh-download"
    rm -f "$tmp"
    if git -C "$SRC" show "$LEGACY_JSON_COMMIT:base/json/$name" > "$tmp" 2>/dev/null && [[ -s "$tmp" ]]; then
      mv "$tmp" "$dst"
      echo "Restored Chromium legacy C++ JSON parser from local git: $name"
    else
      rm -f "$tmp"
      url="https://raw.githubusercontent.com/openFyde/chromium/$LEGACY_JSON_COMMIT/base/json/$name"
      if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 --connect-timeout 15 "$url" -o "$tmp"
      else
        python3 - "$url" "$tmp" <<'PY'
import sys
import urllib.request
url, output = sys.argv[1], sys.argv[2]
with urllib.request.urlopen(url, timeout=30) as response:
    data = response.read()
if not data:
    raise SystemExit("downloaded empty Chromium JSON parser source")
open(output, "wb").write(data)
PY
      fi
      [[ -s "$tmp" ]] || { echo "failed to restore $name" >&2; exit 6; }
      mv "$tmp" "$dst"
      echo "Restored Chromium legacy C++ JSON parser from pinned upstream: $name"
    fi
  fi
done

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
marker = "# KeshOS legacy C++ JSON parser"
if marker not in text:
    old = '''    "json/json_common.h",
    "json/json_reader.cc",
    "json/json_reader.h",
'''
    new = '''    "json/json_common.h",
    # KeshOS legacy C++ JSON parser
    "json/json_parser.cc",
    "json/json_parser.h",
    "json/json_reader.cc",
    "json/json_reader.h",
'''
    if old not in text:
        raise SystemExit("base JSON source block not found")
    path.write_text(text.replace(old, new, 1))
    print("Added Chromium C++ JSON parser sources to //base")
else:
    print("Chromium C++ JSON parser sources already present in //base")
PY

JSON_READER="$SRC/base/json/json_reader.cc"
if grep -q 'serde_json_lenient' "$JSON_READER"; then
  cat > "$JSON_READER" <<'EOF'
// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <string_view>
#include <utility>

#include "base/json/json_parser.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace {
const char kSecurityJsonParsingTime[] = "Security.JSONParser.ParsingTime";
}  // namespace

namespace base {

std::string JSONReader::Error::ToString() const {
  return base::StrCat({"line ", base::NumberToString(line), ", column ",
                       base::NumberToString(column), ": ", message});
}

// static
std::optional<Value> JSONReader::Read(std::string_view json,
                                      int options,
                                      size_t max_depth) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(kSecurityJsonParsingTime);
  internal::JSONParser parser(options, max_depth);
  return parser.Parse(json);
}

// static
std::optional<Value::Dict> JSONReader::ReadDict(std::string_view json,
                                                int options,
                                                size_t max_depth) {
  std::optional<Value> value = Read(json, options, max_depth);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  return std::move(*value).TakeDict();
}

// static
std::optional<Value::List> JSONReader::ReadList(std::string_view json,
                                                int options,
                                                size_t max_depth) {
  std::optional<Value> value = Read(json, options, max_depth);
  if (!value || !value->is_list()) {
    return std::nullopt;
  }
  return std::move(*value).TakeList();
}

// static
JSONReader::Result JSONReader::ReadAndReturnValueWithError(
    std::string_view json,
    int options) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(kSecurityJsonParsingTime);
  internal::JSONParser parser(options);
  auto value = parser.Parse(json);
  if (!value) {
    Error error;
    error.message = parser.GetErrorMessage();
    error.line = parser.error_line();
    error.column = parser.error_column();
    return base::unexpected(std::move(error));
  }
  return std::move(*value);
}

}  // namespace base
EOF
  echo "Switched base::JSONReader to Chromium legacy C++ parser for no-Rust KeshOS"
else
  echo "base::JSONReader already uses the no-Rust C++ parser"
fi

# logging.cc unconditionally includes/calls the generated Rust logging bridge in
# this Chromium revision even when the BUILD dependency has been removed. The
# native C++ logger already handles KeshOS' first-stage logging, so remove only
# that bridge from the no-Rust checkout.
python3 - "$SRC/base/logging.cc" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
include = '#include "base/logging/rust_logger.rs.h"\n'
call = '''  // Connects Rust logging with the //base logging functionality.
  internal::init_rust_log_crate();
'''
marker = '''  // KeshOS no-Rust build: Rust logging bridge intentionally disabled.
'''
changed = False
if include in text:
    text = text.replace(include, '', 1)
    changed = True
if call in text:
    text = text.replace(call, marker, 1)
    changed = True
if changed:
    path.write_text(text)
    print("Removed base Rust logger bridge for no-Rust KeshOS")
elif marker in text:
    print("Base Rust logger bridge already disabled for KeshOS")
else:
    raise SystemExit("base logging Rust bridge pattern not found")
PY
