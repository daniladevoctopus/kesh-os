# Chromium/OpenFyde UI port for KeshOS

Active port of Chromium/OpenFyde's native UI stack onto KeshOS.

Pinned upstream:

- `openFyde/chromium`
- branch `openfyde-r144-dev`
- commit `493c46885be032faa677b4c166e06c7e85a0c396`

KeshOS port branch started from:
`8593410373dcc19213bb46b312daa713968066af`.

## Current path

```
Aura / Views / later Ash
        |
   ui::compositor
        |
      Skia
        |
    OzoneKesh
     /     \
 /dev/fb0  /dev/input/event*
     \     /
      KeshOS
```

KeshOS is not using the Linux kernel. Its userspace already exposes a
Linux-compatible x86_64 syscall ABI to musl, so the first Chromium bring-up
reuses that ABI while adding a real runtime Ozone platform named `kesh`.

## Implemented

- Ozone platform registration: `kesh`
- KeshOS PlatformWindow implementation
- framebuffer-backed software Skia canvas
- display geometry from KeshOS `/dev/fb0`
- keyboard/mouse bridge from KeshOS evdev device nodes
- KeshOS-target GN/Clang x86_64-musl toolchain bridge
- ABI compatibility audit

No KeshOS kernel files have been modified by this port branch so far.

## Prepare an OpenFyde checkout

```bash
./ports/chromium_ui/prepare_build.sh /path/to/openfyde/chromium --gen
```

Without `--gen`, it only installs the overlay and writes
`out/KeshOS/args.gn`.

## Scope

Keep/adapt: base subset, gfx, events, display, Skia, compositor, cc, Viz,
Ozone, Aura, Views, then Ash incrementally.

Do not pull into the shell port: Blink, V8, `content/`, Chrome browser UI,
Linux/ChromiumOS kernel, Google account services, proprietary FydeOS pieces.
