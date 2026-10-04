# KeshOS — Persistent Development Context

**Context version:** 1.0  
**Date:** 2026-09-27  
**Source checklist:** KeshOS — 50 gates до полноценной современной ОС, v2.0

## Working rule

After every user request concerning KeshOS, update this file with:
- what was requested;
- what was changed;
- what was inspected/tested;
- what remains;
- the current next-step queue.

The file is a living development log/context, not a claim of completion. A gate is only marked complete when supported by code, a build, or a real test.

## Current project hierarchy

Boot → Memory → CPU/Interrupts → Process/Userspace Safety → OS ABI → Storage → Input/Graphics → Desktop Userspace → Network/Security → Packages → Installer → Updates/Recovery

### Immediate priority queue

1. Timer / timekeeping hardening
2. ELF64 loader hardening
3. Syscall ABI hardening
4. Generic block layer
5. GPT
6. AHCI / NVMe
7. USB / xHCI
8. File descriptors + IPC + credentials
9. KEA permissions + application sandbox
10. Sockets
11. HTTPS trust store / certificate verification
12. Userspace Desktop migration
13. Package install + signatures + dependencies + transactions
14. Graphical installer
15. Production updates + recovery

Deferred follow-up hardening: per-CPU TSS/kernel stacks, real LAPIC timer scheduling, SMP scheduler migration and runtime SMP stress tests.

## Baseline from checklist v2.0

### Completed

1. Legacy BIOS boot
2. UEFI x86_64 boot
3. Framebuffer graphics
4. Persistent FAT32 read/write/mkdir/delete path on full HDD
5. Working primary Desktop in `src/desktop.c`

### Partial

6. Physical Memory Manager
7. Virtual Memory Manager
8. Page-fault and user-crash handling
12. Threads
13. Per-CPU architecture
14. APIC / IOAPIC
15. SMP / multi-core CPU
16. Timer / timekeeping
17. ELF64 loader
18. Syscall ABI
28. Kernel logging / diagnostics
29. PCI / PCIe device model
34. VFS abstraction
37. Universal input subsystem
38. Audio subsystem
39. Main `desktop.c` migration to userspace
42. Network drivers
43. IPv4 network stack
45. Secure HTTPS trust chain
46. Browser engine
47. KPM network downloader + registry

### Not completed

10. NX / W^X / correct page permissions
19. File descriptor model
20. IPC
21. Users / groups / credentials
22. Privilege / root model
23. Kernel-enforced KEA permissions
24. Application sandbox
25. OS-grade CSPRNG
26. Init / service manager
27. TTY / PTY subsystem
30. ACPI
31. Generic block layer
32. GPT / partition manager
33. AHCI / NVMe
35. Filesystem consistency / journal / fsck
36. USB stack
40. Multi-monitor / DPI / accessibility
41. Hardware GPU acceleration
44. Sockets API
48. Package security / dependencies / atomic install
49. Graphical installer
50. Production updates + recovery

## Current checklist score

7 completed / 21 partial / 22 not completed.

Weighted formula:
- `[x]` = 1.0
- `[~]` = 0.55
- `[ ]` = 0

Current weighted result: ~37.1% (rounded to ~37%).

This percentage is a gate-completion measure, not code coverage or a prediction.

## Desktop migration target

The current `src/desktop.c` is the working native Desktop and remains the parity/reference implementation until the userspace replacement matches it.

### Kernel should retain

- framebuffer/display access
- input collection
- process isolation
- safe surface allocation/mapping
- event delivery
- security/composition boundary
- shutdown/reboot syscall
- permissions/security enforcement

### Userspace should own

- wallpaper
- desktop background
- desktop icons
- top bar
- dock/taskbar
- Start menu
- context menu
- volume popup
- clock presentation
- theme/accent rendering
- app launcher
- settings UI
- most drawing primitives
- userspace window-management policy where appropriate

### Migration principle

**Parity first, deletion second.** Keep the current kernel Desktop working until the userspace Desktop matches visual and behavioral expectations, then remove the old path.

### First Desktop prerequisite

Stabilize a proper safe userspace access layer and then define a public Desktop Surface / Window / Event API rather than exposing physical VRAM directly.

Possible API direction from the existing plan:

```c
int kesh_create_desktop_surface(int width, int height);
uint32_t* kesh_desktop_framebuffer(void);
int kesh_desktop_poll_event(kesh_event_t *ev);
void kesh_desktop_present(void);
int kesh_window_list(...);
int kesh_window_focus(...);
int kesh_window_move(...);
int kesh_window_close(...);
```

The exact API is intentionally not frozen before `copy_from_user/copy_to_user` and core userspace safety are stable.

## Definition of Done

A strong first stable KeshOS release should have at least:

- BIOS + UEFI
- memory isolation
- safe user pointers
- process/thread lifecycle
- modern interrupts + SMP
- modern storage
- USB
- filesystem consistency
- real userspace API
- permissions/sandbox
- network + sockets
- trusted HTTPS
- working userspace Desktop
- package installation
- signed packages
- installer
- updates
- recovery

## Session log

### 2026-09-27 — Context rule established

**User request:** Make the provided KeshOS v2.0 50-gate hierarchy the active development plan and establish a rule that every KeshOS request should be recorded in an `.md` context describing what was done.

**Assistant actions:**
- Saved the persistent working rule to conversation memory.
- Checked the active sandbox for available project files.
- No KeshOS source files were present in `/mnt/data` at this moment, so no source code was modified in this step.
- Created this living context file from the supplied v2.0 checklist and the current development queue.

**Current state:**
- Planning/context: synchronized with the user's supplied v2.0 hierarchy.
- Source-code changes in this session: none.
- Active next task: work from the immediate priority queue, starting with safe userspace memory-copy primitives when the KeshOS source tree is available.


### 2026-09-27 — Gates 6–10 implementation block

**User request:** Take over coding in the sandbox, configure the existing Ninja/BAT/ISO build path, and work through the roadmap five gates at a time.

**Implemented in the source tree:**
- Gate 6 foundation: replaced the PMM-only bump/cursor behavior with a reusable free-range allocator supporting page free, reuse, splitting, coalescing, and double-free/overlap rejection.
- Gate 7 foundation: added page-table inspection, page flag changes, unmap, and recursive destruction of user address spaces with physical page/table cleanup.
- Gate 8 hardening: preserved the existing user-originated fault → process termination path and removed a major class of kernel faults by moving syscall user-buffer access behind validated copy helpers.
- Gate 9: added `user_range_valid()`, `copy_from_user()`, `copy_to_user()`, and bounded `user_strnlen()`. Pointer-bearing syscalls now copy through kernel buffers instead of directly dereferencing user virtual addresses.
- Gate 10 foundation: enabled CPU NXE during syscall initialization, added PTE NX handling, rejected ELF segments that are simultaneously writable and executable, and applied final ELF page permissions plus NX on non-executable mappings. User stacks are mapped writable+NX.
- Process cleanup: process exit/kill paths now destroy the user address space and associated user windows instead of leaving mappings/resources behind.
- ELF cleanup: temporary ELF file buffers are released after loading or on failure.

**Build-system changes:**
- `generate_ninja.py` now emits a real `iso` Ninja target backed by `tools/make_iso.py`.
- `build.bat` now regenerates `build.ninja` before invoking `ninja iso`.
- `build-iso.ps1` now regenerates Ninja and invokes `ninja iso`, so the Windows wrappers use the same dependency graph.

**Verified:**
- `python3 generate_ninja.py && ninja -j4` completed successfully and linked `build/kernel.elf`.
- `ninja -n iso` shows the ISO target is wired into Ninja.
- ELF program headers for `build/apps/notepad.elf` show separate `R E`, `R`, and `RW` LOAD segments with no `W+X` LOAD segment.
- Kernel symbols for `copy_from_user`, `copy_to_user`, `pmm_free_page`, `vmm_destroy_user_pml4`, and `vmm_set_page_flags` are present.
- A second `ninja -j4` completed with no work required after the changes.
- `ninja iso` reached `tools/make_iso.py` but cannot finish in this sandbox because `xorriso` is not installed or present in PATH/toolchain. No QEMU runtime test was performed.

**Gate status after this block:**
- 6 PMM: `[~]` still partial because allocator maturity, fragmentation strategy, and broader leak-resistance testing remain.
- 7 VMM: `[~]` improved substantially with unmap/free/destruction, but demand paging and broader VM features remain.
- 8 Page-fault/user-crash: `[~]` improved because unsafe syscall pointer faults are prevented, but target-runtime fault tests remain.
- 9 safe user copy: `[x]` implemented and compile-verified; runtime target testing remains for final confidence.
- 10 NX/W^X/page permissions: `[~]` implemented for ELF/user mappings, but full architectural verification and broader W^X coverage remain.

**Next queue:**
1. Threads + per-CPU architecture
2. APIC / IOAPIC
3. SMP
4. Generic block layer
5. GPT
6. AHCI / NVMe
7. USB / xHCI
8. File descriptors + IPC + credentials
9. KEA permissions + application sandbox
10. Sockets
11. HTTPS trust store
12. Userspace Desktop migration
13. Package installation/security/transactions
14. Installer
15. Updates + recovery

## Change-log policy

Every subsequent KeshOS interaction should append a dated session entry instead of silently replacing prior history.

## Session log

### 2026-09-27 — Gates 11–15 implementation block

**User request:** Continue coding the next five gates (11–15) while keeping the existing Desktop intact.

**Implemented:**
- Gate 11: strengthened process lifecycle cleanup. `exit/kill` now release the user address space, window state and thread state through a single cleanup path.
- Gate 12: introduced a kernel/user thread model with TIDs, per-thread user contexts, dedicated user stacks for extra threads, scheduler integration, `SYS_THREAD_CREATE` and `SYS_THREAD_EXIT`, plus userspace helpers in `userspace/include/kesh.h`.
- Gate 13: introduced `cpu_local_t`, CPU topology bookkeeping, BSP/AP CPU registration and per-CPU scheduler counters.
- Gate 14: introduced xAPIC/IOAPIC initialization, MADT discovery through the Limine RSDP request, ISA IRQ routing, APIC EOI handling and PIC fallback switching.
- Gate 15: wired Limine SMP topology discovery and AP bring-up callbacks. APs load the shared IDT, initialize their local APIC state and enter an interrupt-enabled idle loop.

**Build/test evidence:**
- `python3 generate_ninja.py` completed successfully.
- `ninja -j4` completed successfully and linked `build/kernel.elf`.
- `nm build/kernel.elf` confirmed the new CPU/APIC/thread symbols.
- `.requests` section remains present in `kernel.elf`, containing the SMP/RSDP request objects.
- ISO generation was attempted with `ninja iso` but the sandbox does not contain `xorriso`; therefore the new ISO and QEMU runtime boot are **not** claimed as tested.
- QEMU is also not installed in the sandbox environment.

**Honest gate status after this block:**
- 11 → `[x]`
- 12 → `[~]` because runtime user-thread execution has not been boot-tested and the kernel still shares the syscall/TSS stack path.
- 13 → `[~]` because the per-CPU data architecture exists, but TSS/kernel-stack/GS isolation is still global and must be hardened before a full `[x]`.
- 14 → `[~]` because APIC/IOAPIC code and routing are built, but hardware/runtime verification is unavailable in the sandbox.
- 15 → `[~]` because Limine SMP AP bring-up is wired, but multi-core runtime verification and SMP scheduling are still pending.

**Source files added/changed:**
- `kernel/cpu.h`
- `kernel/cpu.c`
- `kernel/apic.h`
- `kernel/apic.c`
- `kernel/process.h`
- `kernel/process.c`
- `kernel/syscall.h`
- `kernel/syscall.c`
- `kernel/include/idt.h`
- `kernel/include/idt.c`
- `kernel/include/pic.h`
- `kernel/include/pic.c`
- `kernel/kernel.c`
- `userspace/include/kesh.h`
- `generate_ninja.py`

**Next block:** gates 16–20, beginning with timekeeping, ELF/syscall hardening, then file descriptors/IPC groundwork.


### 2026-09-29 — Linux build + black-screen regression hardening

**User request:** The newly rebuilt KeshOS shows a black screen on boot. Audit the recent changes, make the project easy to build on a fresh CachyOS/Linux install, and provide one `.sh` script that builds the project and packages a bootable ISO.

**Root-cause finding:**
- The 11–15 changes unconditionally switched IRQ delivery from the legacy PIC to APIC mode and masked the PIC even when a usable IOAPIC/ACPI path was not safely established.
- In that failure case the PIT can stop generating the timer interrupts used by the boot progress loop, producing a permanent black/partially initialized framebuffer state.
- The recent SMP path also attempted to prepare AP execution before per-CPU GDT/TSS and scheduler state were production-ready. This was too aggressive for the current kernel architecture.
- This is a statically identified regression path that matches the observed symptom. The sandbox still has no QEMU, so a visual target boot could not be used to prove the fix end-to-end.

**Code changes:**
- `kernel/apic.c/.h`: APIC initialization now has a fail-safe return path; it requires APIC support, an HHDM mapping, a valid Limine RSDP response, a usable MADT/IOAPIC, and rejects Limine x2APIC mode until a dedicated x2APIC backend exists.
- IOAPIC IRQ destinations now use the real BSP LAPIC ID instead of hard-coded destination 0.
- Legacy PIC is masked only after successful APIC/IOAPIC setup. Otherwise the existing PIT/PIC path remains active.
- `kernel/cpu.c/.h`: Limine's single SMP request is the source of truth; BSP LAPIC ID comes from Limine when available. AP execution callbacks are now deliberately deferred until per-CPU GDT/TSS and scheduler state are ready.
- `kernel/kernel.c`: logs APIC fallback instead of assuming APIC success.
- `generate_ninja.py`: tool discovery is now cross-platform. `CC`, `CXX`, `LD`, `AR`, and `KESHOS_TOOLCHAIN` can be supplied from the environment; otherwise PATH is used, with the existing Windows toolchain path retained as a fallback. Ninja uses the exact Python interpreter running `generate_ninja.py` for `.kea` packaging.
- `tools/make_iso.py`: xorriso lookup is now Linux/Windows neutral and gives a clear missing-dependency error.
- Added `build.sh` for Linux/CachyOS. `./build.sh` checks dependencies, regenerates Ninja, performs a clean generated build, builds all targets, and creates `build/keshos.iso` through `ninja iso`. `./build.sh --install-deps` installs the documented CachyOS/Arch dependencies with `pacman`.
- `Makefile` now uses `python3`, Ninja and the Linux `build.sh` flow; `build.bat` and `build-iso.ps1` keep their Windows behavior with PATH/custom-toolchain fallback.
- README files now document the fresh CachyOS setup and one-command Linux build.

**Verified in sandbox:**
- `python3 -m py_compile generate_ninja.py tools/make_iso.py` passed.
- `bash -n build.sh` passed.
- `python3 generate_ninja.py` passed.
- `ninja -j4` completed successfully and linked `build/kernel.elf`.
- ELF LOAD segments remain split as `R E`, `R`, and `RW`, with no writable+executable LOAD segment.
- The generated Ninja graph uses the actual Python interpreter for `.kea` packaging.
- Only one `LIMINE_SMP_REQUEST` exists in the kernel sources.
- The final sandbox still lacks `xorriso` and QEMU, so ISO creation and visual runtime boot remain unverified here.

**Current checklist score:** 7 completed / 21 partial / 22 not completed = ~37.1%. No new gate is promoted to `[x]` by this corrective pass; the APIC/SMP gates remain `[~]` until real runtime verification.

**Next queue:**
1. Gates 16–20
2. Use the fresh CachyOS build script to perform real BIOS/UEFI/QEMU boot testing
3. After boot regression is cleared, continue threads/per-CPU hardening before enabling AP startup
4. Keep the kernel Desktop unchanged until the userspace migration stage


### 2026-09-29 — Raw serial log + second black-screen fix

**User request:** The user supplied `serial.log` showing a black-screen boot and asked for a real kernel-generated diagnostic log, a fix based on the evidence, and a persistent `log.md` diary containing each future user request followed by the engineering decisions and changes.

**Observed evidence:** The supplied log reaches framebuffer acquisition, screen clear and boot-logo fade, then stops before `[5/6]` and `[6/6]`. The visible timestamps remain `0.000`, making the first timer-dependent wait in the boot logo path the primary runtime suspect. The early log also contains genuinely concatenated records caused by mixed `KLOG_*` and raw `serial_print*` calls.

**Important correction:** The earlier hypothesis that `stack_ptr[15]` was the wrong IRQ vector slot was rechecked against `kernel/isr_stubs.S` and rejected. The existing stub layout places the vector at slot 15 for the common IRQ frame.

**Changes now applied:**
- Kept APIC discovery/initialization available but stopped switching the verified IRQ path to APIC mode. The kernel keeps PIC/PIT active until timer, keyboard and mouse routing are migrated and runtime-verified.
- Added real timer IRQ counting and first-four PIT IRQ traces in `kernel/timer.c`.
- Added bounded IRQ dispatcher traces in `kernel/include/pic.c` reporting vector, line, controller and handler presence.
- Upgraded kernel log records with runtime milliseconds, timer ticks, CPU index and source `file:line`.
- Fixed several log-formatting points so numeric fields no longer get appended after a subsequent log prefix.
- Added a `[KERNEL-SERIAL]` runtime-stream banner.
- Added `run-qemu-log.sh` to capture COM1 output into `serial.log` during a real QEMU boot.
- Corrected the APIC status message so it accurately states that the legacy PIC remains active for the verified PIT/IRQ path.

**Verification:** Python syntax checks, shell syntax checks, Ninja graph generation and `ninja -j4` pass. `kernel.elf` contains the new timer/IRQ diagnostic symbols and strings. The sandbox does not contain QEMU or xorriso, so the actual repaired boot is not claimed as verified here.

**Current next step:** Run the rebuilt ISO on CachyOS/QEMU or VirtualBox and send the resulting `serial.log` if it still stops. The first decisive evidence is whether `vector=32 line=0 controller=PIC handler=yes` and growing `IRQ0`/`ticks` records appear. If timer evidence is healthy while the screen remains black, the investigation moves directly to framebuffer/logo drawing and its memory access path.

## 2026-09-29 — Black-screen tracing pass

- User supplied a fresh `serial.log`; it stops at splash `[4/6]` immediately after framebuffer clear/logo-fade start.
- Rejected the earlier wrong hypothesis that the IRQ vector stack index was wrong; `stack_ptr[15]` matches the actual ISR stub layout.
- Deferred LAPIC/IOAPIC activation completely during legacy PIC/PIT boot. APIC remains available for later migration but is no longer enabled on the boot-critical path.
- Added a dedicated seven-level kernel logger (`L1 FATAL` through `L7 TRACE`) with runtime sequence, ms, ticks, CPU, TSC, CR3, IF, module, `file:line`, and `function()`.
- Added `KLOG_ENTER/LEAVE` tracing for important micro-functions and direct IRQ0 handler tracing.
- Made `timer_wait_ticks()` explicitly enable interrupts before `hlt`, restoring the caller's original IF state afterward.
- `cpu_current_index()` now uses CPUID APIC ID until LAPIC is enabled.
- QEMU serial capture is now written directly to a file to avoid ANSI/terminal interleaving.
- Build verification: `generate_ninja.py`, full `ninja -j4`, Python syntax checks, shell syntax checks, ISO dry-run graph, and ELF permission inspection all pass.
- Runtime QEMU/ISO validation is still pending because the sandbox lacks QEMU/xorriso.

### 2026-09-29 — Deep trace / serial transport / timer fallback pass

- User supplied another black-screen log and requested a ReactOS-style seven-level kernel log with function/file/line information and a persistent `log.md` diary.
- The supplied log stops at splash `[4/6]` and is heavily interleaved, so its final execution point cannot be trusted beyond the visible stage boundary. fileciteturn4file0L21-L24
- Rechecked the IRQ stub and kept vector slot 15; the earlier stack-index hypothesis remains rejected.
- Kept PIC/PIT active for the boot-critical path instead of enabling APIC/IOAPIC prematurely.
- Added `L1..L7` kernel logger with live sequence, time, ticks, CPU, TSC, CR3, IF, module, `file:line`, and `function()` plus `ENTER/LEAVE` tracing.
- Added `kernel/serial.c` + `kernel/serial.h` as the single normal COM1 output path. UART transmission now waits for THR-empty to reduce serial record corruption/interleaving.
- Added TSC-based monotonic clock detection via CPUID 0x15/0x16 and a deadline fallback for `timer_wait_ticks()`, so boot splash timing cannot depend exclusively on PIT IRQ0 delivery.
- Full `ninja -j4` build passed after the changes. QEMU/xorriso runtime verification remains unavailable in the sandbox.
- `log.md` now contains the full user request and the engineering journal for this pass.

## 2026-09-29 — Black-screen V4 runtime identity and safe splash

The user supplied another runtime log that again stops at splash stage `[4/6]` and contains multiple concatenated boot sequences. The supplied log has no records from the new `[KLOG ... L7]` logger, so it is not treated as proof that the newest kernel image was booted.

Implemented V4 debugging/fix path:
- runtime build fingerprint `KESHOS-2026-09-29-BLACKSCREEN-V4`;
- source-location logger records (`level`, `seq`, `ms`, `tick`, `cpu`, `tsc`, `cr3`, `rsp`, `rflags`, module/file/line/function);
- serialized COM1 log records;
- framebuffer four-corner probe;
- timer-independent, bounded boot splash rendering;
- SHA-256 ISO-to-kernel verification to detect stale ISO images;
- QEMU low-level interrupt/reset trace capture.

No gate is marked complete from this debugging pass. Runtime proof remains required on the user's actual CachyOS/QEMU environment.


## 2026-10-04 — OpenFyde Chromium UI / OzoneKesh port

**Goal:** port the native Chromium/OpenFyde graphics and shell stack to KeshOS
without importing Blink/V8/content or the Linux/ChromiumOS kernel.

**Pinned upstream:** `openFyde/chromium` branch `openfyde-r144-dev`, commit
`493c46885be032faa677b4c166e06c7e85a0c396`.

**Dedicated branch:** `port/openfyde-ozone-kesh`.

**Implemented on this branch:**
- additive Chromium `ozone_extra_path` overlay registering platform `kesh`;
- Kesh framebuffer backend using existing `/dev/fb0` mmap/ioctl support;
- software Skia SurfaceOzoneCanvas presentation into the KeshOS framebuffer;
- Kesh PlatformWindow + basic screen/window manager;
- KeshEventSource reading existing `/dev/input/event0` and `event1`;
- Chromium KeyEvent/MouseEvent translation using Chromium evdev key converters;
- dedicated GN target toolchain targeting `x86_64-unknown-linux-musl` with the
  KeshOS sysroot;
- build preparation and smoke-build scripts;
- compatibility audit of `kernel/linux_syscall.c`.

**Key finding:** KeshOS already has most primitives needed by Chromium's Linux
base message loop, including epoll, eventfd, clone/futex, VM syscalls, clocks,
poll, memfd and file-descriptor I/O. Chromium r144 MessagePumpEpoll therefore
has a plausible path without new kernel work.

**Current deliberate bootstrap:** Chromium is compiled with Linux/POSIX build
assumptions because KeshOS already exposes that musl-compatible syscall ABI,
but runtime graphics/input use the new native Ozone platform `kesh`. This is
not a Linux-kernel dependency.

**Kernel status:** no kernel/user ABI files were modified for this port yet.

**Next executable milestone:** build upstream `ui/ozone/demo:ozone_demo` with
`--disable-gpu --ozone-platform=kesh`, stage it as
`ports_bin/ozone_demo.elf`, boot it on real KeshOS, then fix only the concrete
syscall/Ozone failures it exposes. Aura and Views come after that, Ash after
Views.


### 2026-10-04 — OzoneKesh build-boundary hardening

- Rechecked OpenFyde r144 Ozone APIs and the KeshOS Linux-compatible syscall path before the first real build.
- Confirmed KeshOS `linux_syscall.c` routes file-backed `mmap()` for `FD_KIND_DRM_FB` to `drm_fb_mmap()`, so the current OzoneKesh framebuffer mmap path is backed by the KeshOS kernel rather than Linux.
- Confirmed Chromium's Ozone software renderer reaches `SurfaceFactoryOzone::CreateCanvasForWidget()`, matching the current KeshSurfaceFactory design.
- Fixed the custom Chromium target toolchain so clang is explicitly passed `--sysroot=<KeshOS musl sysroot>` for C, C++ and linking. A target triple alone was not sufficient to prevent host headers/libraries leaking into target objects.
- Added early sysroot checks for `linux/fb.h`, `linux/input.h` and static musl `libc.a` before starting an expensive Chromium build.
- No KeshOS kernel or userspace ABI source was changed in this pass.
- Next milestone remains the first actual `ozone_demo` build/run, then concrete fixes from compiler/runtime errors before moving to Aura/Views.


### 2026-10-04 — First conservative kernel change for Chromium/OpenFyde compatibility

The user explicitly allowed small, gradual KeshOS kernel changes for the port.

Implemented on `port/openfyde-ozone-kesh`:
- added Linux ABI syscall `prctl(2)` number 157;
- implemented `PR_SET_NAME` / `PR_GET_NAME` with per-thread 16-byte Linux-style names;
- implemented `PR_SET_DUMPABLE` / `PR_GET_DUMPABLE` as per-process state;
- Linux-created threads inherit the current thread name;
- new processes initialize the main thread name from the process name and default to dumpable=1.

Why this change was chosen:
- OpenFyde Chromium r144 directly calls `prctl(PR_SET_NAME)` in
  `base/threading/platform_thread_linux.cc`;
- its POSIX stack-trace code calls `PR_GET_DUMPABLE` and
  `PR_SET_DUMPABLE`;
- this closes a concrete Chromium base ABI gap without changing scheduler,
  graphics, memory-management or security-permission architecture.

Signals are deliberately NOT expanded in the same commit. KeshOS still has
stub `rt_sigaction` / `rt_sigprocmask`, and proper signal semantics will be
handled separately rather than pretending a broad signal implementation exists.

### 2026-10-04 — OpenFyde/OzoneKesh bring-up sprint for first visible frame

**User request:** Broad permission was granted to change KeshOS and the OpenFyde port as needed, with the immediate goal of making a first visible Chromium/OpenFyde-native result practical to compile and boot on the user's CachyOS development machine.

**Port strategy:** The first executable milestone was reduced from the full upstream `ozone_demo` to a purpose-built `//keshos/ozone:kesh_smoke`. This keeps real Chromium `base`, Skia, Ozone and PlatformWindow code while deliberately excluding Blink, V8, Chrome, the GL demo renderer and Mojo initialization. The target draws a dark 900x560 test shell with three content cards, an orange accent and a moving green heartbeat strip.

**KeshOS/OpenFyde changes made:**
- added a pinned OpenFyde/depot_tools bootstrap using `gclient` and revision `493c46885be032faa677b4c166e06c7e85a0c396`;
- added a KeshOS Chromium GN toolchain that uses the KeshOS musl sysroot and emits static non-PIE ET_EXEC output;
- added ELF preflight validation for PT_INTERP/PT_DYNAMIC and W+X LOAD segments;
- made the OzoneKesh framebuffer/input ABI self-contained instead of requiring Linux UAPI header packages;
- added `kesh_smoke` and the one-command `ports/chromium_ui/tomorrow_build.sh` bring-up path;
- restored missing `.gitmodules` metadata for the repository's pinned BearSSL gitlink so fresh clones can reproduce kernel builds;
- fixed the duplicate `prctl(2)` dispatch exposed by CI;
- expanded framebuffer mmap safety for high-resolution framebuffers;
- added exclusive `/dev/fb0` scanout ownership. While a Ring 3 compositor owns fb0, the legacy kernel desktop continues input/network/services/process scheduling but stops swapping frames over userspace output. Ownership is released automatically when the owning process closes its last independently opened fb0 FD or exits.

**Build evidence:** GitHub Actions successfully builds `build/kernel.elf` on the port branch after the `prctl` and BearSSL fixes, and validates the kernel ELF program headers. The Chromium/OpenFyde cross-build and on-device `kesh_smoke` runtime are still explicitly unverified because the ChatGPT container cannot perform the full OpenFyde checkout/build.

**Tomorrow test:** switch to `port/openfyde-ozone-kesh`, run `./ports/chromium_ui/tomorrow_build.sh`, boot `build/keshos.iso`, open Terminal and run `run /boot/apps/kesh_smoke.elf`. Save the final build output and `serial.log` on failure. Full Ash remains after the sequence `kesh_smoke -> upstream ozone_demo -> Aura -> Views -> Ash`.
