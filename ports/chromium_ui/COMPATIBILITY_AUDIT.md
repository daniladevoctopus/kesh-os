# KeshOS / Chromium base compatibility audit

Source baseline: KeshOS main at `8593410373dcc19213bb46b312daa713968066af`.

## Important result

KeshOS already contains a substantial x86_64 Linux-compatible syscall ABI for
musl in `kernel/linux_syscall.c`. The Ozone port therefore does not need to
invent a libc ABI before the first Aura/Views test.

This compatibility ABI is implemented by the KeshOS kernel. The runtime
graphics/input platform remains native OzoneKesh.

## Present and useful for Chromium base

The current dispatcher covers these families:

- read / write / readv / writev
- open / openat / close / stat / fstat / statx / getdents64
- lseek / fcntl / dup / dup2
- mmap / mprotect / munmap / mremap / brk / madvise
- clone / gettid / set_tid_address / futex / sched_yield
- nanosleep / clock_nanosleep / clock_gettime
- pipe / pipe2
- memfd_create / ftruncate
- eventfd2
- poll / ppoll
- epoll_create / epoll_create1 / epoll_ctl / epoll_wait / epoll_pwait
- getrandom
- ioctl
- AF_UNIX socket primitives
- arch_prctl
- uname / getcwd / chdir / readlink

OpenFyde Chromium r144's `MessagePumpEpoll` uses `epoll_create1`,
`eventfd`, `epoll_ctl`, `epoll_wait`, read and write. KeshOS already has
those syscall paths.

## Known weak spots

- rt_sigaction / rt_sigprocmask are currently stubs
- pselect6 only supports its timeout-only form
- Linux socket ABI currently accepts AF_UNIX rather than the full network set
- no timerfd syscall was found
- no inotify syscall family was found
- no prctl syscall was found
- several fcntl operations are compatibility stubs
- DRM/KMS support is intentionally tiny and framebuffer-oriented

## Decision

Do not change the KeshOS kernel yet.

First attempt:

1. KeshOS musl ABI
2. KeshOS GN target toolchain
3. OzoneKesh software framebuffer
4. KeshEventSource
5. Aura/Views smoke target

Only add kernel syscalls after a concrete build/runtime failure proves one is
required.
