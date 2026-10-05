#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
FILE="$SRC/base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_internals.h"

python3 - "$FILE" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
old = '''#if PA_BUILDFLAG(IS_POSIX)
#include <sys/cdefs.h>  // for __THROW
#endif
'''
new = '''#if PA_BUILDFLAG(IS_POSIX)
// glibc exposes __THROW from <sys/cdefs.h>, but musl deliberately does not
// provide that glibc compatibility header. Chromium already has a fallback
// definition below, so only include cdefs when the target libc has it.
#if __has_include(<sys/cdefs.h>)
#include <sys/cdefs.h>  // for __THROW
#endif
#endif
'''

if new in text:
    print("PartitionAlloc sys/cdefs.h include already guarded for musl")
elif old in text:
    path.write_text(text.replace(old, new, 1))
    print("Guarded PartitionAlloc sys/cdefs.h include for musl/KeshOS")
else:
    raise SystemExit("PartitionAlloc cdefs include block not found")
PY
