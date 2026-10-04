# Port status

## M0 - source and boundary audit
Status: DONE

KeshOS already provides framebuffer/DRM-like devices, evdev, FDs/poll,
threads, clocks, VM mapping, IPC and a musl userspace.

## M1 - OzoneKesh software display backend
Status: INITIAL IMPLEMENTATION DONE

Rendering path:

```
Skia raster surface
  -> SurfaceOzoneCanvas::PresentCanvas
  -> KeshFramebuffer::Blit
  -> mmap(/dev/fb0)
  -> KeshOS framebuffer
```

Implemented: OzonePlatformKesh, KeshWindowManager, KeshWindow, KeshScreen,
KeshSurfaceFactory and KeshFramebuffer.

## M2 - native input
Status: INITIAL IMPLEMENTATION DONE

KeshEventSource reads KeshOS `/dev/input/event0` and `event1`, translates
evdev key codes with Chromium's own converters and dispatches KeyEvent,
MouseEvent and MouseWheelEvent.

Current implementation polls every 4 ms on the UI thread. This is intentionally
simple for bring-up and requires no kernel change.

## M3 - Chromium base / toolchain bridge
Status: INITIAL AUDIT + TOOLCHAIN DONE

Found in the KeshOS Linux-compatible musl ABI:

- clone/futex/thread primitives
- mmap/mprotect/munmap
- epoll/eventfd
- poll/ppoll
- clocks/sleep
- memfd
- open/read/write/ioctl
- getrandom
- AF_UNIX IPC

OpenFyde r144 MessagePumpEpoll's core requirements line up with existing KeshOS
epoll/eventfd/read/write paths.

Added `//keshos/toolchain:kesh_x64` so the target build uses
`x86_64-unknown-linux-musl` and the KeshOS sysroot while host build tools stay
native to CachyOS.

No KeshOS kernel change has been made.

## M4.0 - Ozone smoke executable
Status: READY TO BUILD, NOT RUNTIME-VERIFIED

Use Chromium's upstream `ui/ozone/demo:ozone_demo` first. It is a much smaller
test than Aura/Views and can exercise:

- OzoneKesh selection
- PlatformWindow creation
- software Skia canvas
- framebuffer presentation
- keyboard/mouse dispatch
- Chromium MessagePumpEpoll on KeshOS

Run:

```bash
./ports/chromium_ui/build_smoke.sh /path/to/openfyde/chromium
```

The script stages the result as `ports_bin/ozone_demo.elf`; KeshOS ISO
packaging already copies `ports_bin/*.elf` into `/boot/apps/`.

Runtime args: `--disable-gpu --ozone-platform=kesh`.

## M4.1 - Aura smoke test
Status: NOT STARTED

Only after ozone_demo actually runs on KeshOS.

## M4.2 - Views smoke test
Status: NOT STARTED

Target: a basic Views window rendered through Aura -> compositor -> OzoneKesh.

## M5 - Ash
Status: NOT STARTED

Order after Views works:
window management -> shelf -> launcher -> tray -> notifications.
