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

Files in `chromium_overlay/keshos/ozone/` implement:

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
Status: NEXT

Use KeshOS `/dev/input/event0` and `/dev/input/event1` to feed Chromium
KeyEvent/MouseEvent through PlatformEventSource. No kernel change is currently
requested.

## M3 - Chromium base compatibility
Status: NOT STARTED

Audit Linux/POSIX assumptions in Chromium base. Prefer userspace compatibility
shims. Ask before changing KeshOS kernel/userspace ABI.

## M4 - Aura + Views smoke test
Status: NOT STARTED

Target: a single Views window rendered by OzoneKesh.

## M5 - Ash
Status: NOT STARTED

Only after M4 works:
window manager -> shelf -> launcher -> tray -> notifications.
