# Port status

## M0 - source and boundary audit
Status: DONE

KeshOS already provides framebuffer/DRM-like devices, evdev, FDs/poll,
threads, clocks, VM mapping, IPC and a musl userspace.

Pinned OpenFyde Chromium revision:
`493c46885be032faa677b4c166e06c7e85a0c396` (`openfyde-r144-dev`).

## M1 - OzoneKesh software display backend
Status: INITIAL IMPLEMENTATION DONE

Rendering path:

```
Chromium / Skia raster surface
  -> SurfaceOzoneCanvas::PresentCanvas
  -> KeshFramebuffer::Blit
  -> mmap(/dev/fb0)
  -> KeshOS framebuffer
```

Implemented: OzonePlatformKesh, KeshWindowManager, KeshWindow, KeshScreen,
KeshSurfaceFactory and KeshFramebuffer.

The backend now carries its small KeshOS framebuffer/input ABI definitions in
`kesh_linux_abi.h` instead of depending on Linux UAPI header packages.

## M2 - native input
Status: INITIAL IMPLEMENTATION DONE

KeshEventSource reads KeshOS `/dev/input/event0` and `event1`, translates the
KeshOS evdev-compatible records with Chromium key converters and dispatches
KeyEvent, MouseEvent and MouseWheelEvent.

Current implementation polls every 4 ms on the UI thread. This is intentionally
simple for first bring-up. Event-driven FD watching comes after the first frame.

## M3 - Chromium base / toolchain bridge
Status: INITIAL IMPLEMENTATION DONE, RUNTIME VERIFICATION PENDING

KeshOS' Linux-compatible musl ABI already contains the core primitives needed by
Chromium base: clone/futex, VM syscalls, epoll/eventfd, poll, clocks, memfd,
file I/O, ioctl, getrandom and AF_UNIX IPC.

Additional compatibility work on the port branch includes Chromium-required
`prctl` thread-name/dumpable state and safer framebuffer mappings.

The custom GN toolchain targets `x86_64-unknown-linux-musl` only as a userspace
ABI bridge. It explicitly uses the KeshOS musl sysroot and emits a static,
non-PIE ET_EXEC binary because the KeshOS ELF loader rejects PT_INTERP,
PT_DYNAMIC and W+X LOAD segments.

## M4.0 - minimal Chromium-native visual smoke
Status: READY FOR FIRST REAL CROSS-BUILD, NOT RUNTIME-VERIFIED

The first target is now our deliberately small `//keshos/ozone:kesh_smoke`, not
Chromium's heavier upstream ozone_demo. It avoids Blink, V8, Chrome, GL renderer
and Mojo initialization while still exercising:

- Chromium `base::MessagePump` / timer
- OpenFyde/Chromium Ozone platform selection
- PlatformWindow creation
- Skia software rasterization
- OzoneKesh framebuffer presentation

Expected visible result: a dark 900x560 Chromium/Skia test shell with an orange
accent, three cards and a moving green heartbeat strip.

Build everything with:

```bash
./ports/chromium_ui/tomorrow_build.sh
```

After booting `build/keshos.iso`, open KeshOS Terminal and run:

```text
run /boot/apps/kesh_smoke.elf
```

The first source/dependency sync can be large and slow. Build parallelism
defaults to 2 jobs for the current low-core development machine.

## M4.1 - upstream ozone_demo
Status: AFTER KESH_SMOKE

Once the minimal smoke target is alive, build Chromium's upstream ozone_demo to
exercise more of the stock Ozone renderer stack.

## M4.2 - Aura smoke test
Status: NOT STARTED

Create a root Aura window and prove compositor/input routing through OzoneKesh.

## M4.3 - Views smoke test
Status: NOT STARTED

Target: a basic Views widget rendered through Aura -> compositor -> OzoneKesh.

## M5 - Ash
Status: NOT STARTED

Only after Views is stable:
window management -> shelf -> launcher -> tray -> notifications.
