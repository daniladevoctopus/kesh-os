#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openfyde/chromium" >&2
  exit 2
fi

SRC="$(cd "$1" && pwd)"
INTERNALS="$SRC/base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_internals.h"
DISPATCH="$SRC/base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.cc"
PROCESS_METRICS="$SRC/base/process/process_metrics_posix.cc"

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

# Chromium's Linux ProcessMetrics assumes every Linux libc has mallinfo().
# musl intentionally does not implement mallinfo/mallinfo2, so the KeshOS
# cross target must not compile that glibc-only path. Returning 0 is already a
# supported fallback semantics for platforms where malloc usage is unavailable.
python3 - "$PROCESS_METRICS" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
marker = "// KeshOS musl: mallinfo is unavailable."

if marker in text:
    print("ProcessMetrics mallinfo fallback already ready for musl")
else:
    old = '''size_t GetMallocUsageMallinfo() {
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
#define MALLINFO2_FOUND_IN_LIBC
  struct mallinfo2 minfo = mallinfo2();
#endif
#endif  // defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if !defined(MALLINFO2_FOUND_IN_LIBC)
  struct mallinfo minfo = mallinfo();
#endif
#undef MALLINFO2_FOUND_IN_LIBC
  return checked_cast<size_t>(minfo.hblkhd + minfo.arena);
}
'''
    new = '''size_t GetMallocUsageMallinfo() {
#if !defined(__GLIBC__) && !defined(__ANDROID__)
  // KeshOS musl: mallinfo is unavailable.
  // Memory accounting can be wired to a native KeshOS allocator API later.
  return 0;
#else
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
#define MALLINFO2_FOUND_IN_LIBC
  struct mallinfo2 minfo = mallinfo2();
#endif
#endif  // defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if !defined(MALLINFO2_FOUND_IN_LIBC)
  struct mallinfo minfo = mallinfo();
#endif
#undef MALLINFO2_FOUND_IN_LIBC
  return checked_cast<size_t>(minfo.hblkhd + minfo.arena);
#endif
}
'''
    if old not in text:
        raise SystemExit("Chromium ProcessMetrics mallinfo block not found")
    path.write_text(text.replace(old, new, 1))
    print("Disabled Chromium mallinfo metrics on musl KeshOS target")
PY
