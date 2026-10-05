#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
INTERNALS="$SRC/base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_internals.h"
DISPATCH="$SRC/base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.cc"

python3 - "$INTERNALS" "$DISPATCH" <<'PY'
from pathlib import Path
import sys

internals = Path(sys.argv[1])
dispatch = Path(sys.argv[2])

text = internals.read_text()
old_include = '''#if PA_BUILDFLAG(IS_POSIX)
#include <sys/cdefs.h>  // for __THROW
#endif
'''
guarded_include = '''#if PA_BUILDFLAG(IS_POSIX)
// glibc exposes __THROW from <sys/cdefs.h>, while musl deliberately does not
// provide that glibc compatibility header. Only include it when available.
#if __has_include(<sys/cdefs.h>)
#include <sys/cdefs.h>  // for __THROW
#endif
#endif
'''
previous_guarded_include = '''#if PA_BUILDFLAG(IS_POSIX)
// glibc exposes __THROW from <sys/cdefs.h>, but musl deliberately does not
// provide that glibc compatibility header. Chromium already has a fallback
// definition below, so only include cdefs when the target libc has it.
#if __has_include(<sys/cdefs.h>)
#include <sys/cdefs.h>  // for __THROW
#endif
#endif
'''

if old_include in text:
    text = text.replace(old_include, guarded_include, 1)
elif previous_guarded_include in text:
    text = text.replace(previous_guarded_include, guarded_include, 1)
elif guarded_include not in text:
    raise SystemExit("PartitionAlloc cdefs include block not found")

old_throw = '''#ifndef __THROW   // Not a glibc system
#ifdef _NOEXCEPT  // LLVM libc++ uses noexcept instead
#define __THROW _NOEXCEPT
#else
#define __THROW
#endif  // !_NOEXCEPT
#endif
'''
new_throw = '''#ifndef __THROW  // musl and other non-glibc libcs
// Match the declarations provided by musl's malloc/stdlib headers. Those
// declarations do not carry glibc's C++ noexcept annotation.
#define __THROW
#endif
'''
if old_throw in text:
    text = text.replace(old_throw, new_throw, 1)
elif new_throw not in text:
    raise SystemExit("PartitionAlloc __THROW fallback block not found")

internals.write_text(text)
print("PartitionAlloc cdefs/__THROW compatibility ready for musl")

text = dispatch.read_text()
old_mallinfo = '''#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
'''
new_mallinfo = '''#if (PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
'''
if old_mallinfo in text:
    text = text.replace(old_mallinfo, new_mallinfo, 1)
elif new_mallinfo not in text:
    raise SystemExit("PartitionAlloc mallinfo block not found")

dispatch.write_text(text)
print("PartitionAlloc glibc-only mallinfo shim disabled for musl")
PY
