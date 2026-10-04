# Port status

## M0 - source and boundary audit
Status: DONE

KeshOS already has the primitives needed for the first Ozone bring-up:

- framebuffer device and mmap
- DRM-like device node
- evdev keyboard/mouse device nodes
- FD API and poll
- VM map/unmap
- threads and clocks
- IPC
- Ring 3 userspace with musl/libc++ porting work

## M1 - OzoneKesh software display backend
Status: IMPLEMENTED AS INITIAL PATCH

Implemented:

- OzonePlatformKesh
- KeshWindowManager
- KeshWindow
- KeshScreen
- KeshSurfaceFactory
- KeshFramebuffer

Rendering path:

```
Skia raster surface
  -> SurfaceOzoneCanvas::PresentCanvas
  -> damage clipped to window/display
  -> KeshFramebuffer::Blit
  -> mmap(/dev/fb0)
  -> KeshOS framebuffer
```

## M2 - native input
Status: INITIAL IMPLEMENTATION DONE

`KeshEventSource` now:

- opens KeshOS `/dev/input/event0` and `/dev/input/event1`
- consumes KeshOS Linux-compatible `input_event` records
- translates evdev key codes using Chromium's own KeycodeConverter
- dispatches Chromium KeyEvent / MouseEvent / MouseWheelEvent
- maintains keyboard modifiers and mouse button flags
- exposes the live logical cursor location to KeshScreen

The first implementation uses a 4 ms UI-thread polling timer. This avoids any
kernel change and matches KeshOS's current non-blocking evdev queues. Replace
with fd-watcher/poll integration after the basic Views smoke test if needed.

## M3 - Chromium base compatibility
Status: NEXT

Audit Linux/POSIX assumptions in Chromium base against the existing KeshOS musl
port. Prefer userspace compatibility shims. Ask before changing KeshOS
kernel/userspace ABI.

## M4 - Aura + Views smoke test
Status: NOT STARTED

Target: a single Views window rendered by OzoneKesh with working mouse and
keyboard.

## M5 - Ash
Status: NOT STARTED

Only after M4 works:
window manager -> shelf -> launcher -> tray -> notifications.
