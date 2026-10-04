# Chromium/OpenFyde UI port for KeshOS

This directory contains the KeshOS-specific Ozone platform overlay used to bring
Chromium's native UI stack (Ozone -> Aura -> Views -> Ash) onto KeshOS.

Upstream pin:

- repository: openFyde/chromium
- branch: openfyde-r144-dev
- commit: 493c46885be032faa677b4c166e06c7e85a0c396

KeshOS baseline for this branch:

- repository: daniladevoctopus/kesh-os
- commit: 8593410373dcc19213bb46b312daa713968066af

## What this first port does

The first milestone is intentionally software-only.

- registers a new Ozone platform named `kesh`
- creates Chromium PlatformWindow objects backed by KeshOS window metadata
- exposes the KeshOS framebuffer as the software Ozone output
- creates a Chromium PlatformScreen from /dev/fb0 geometry
- uses Skia raster surfaces and copies damaged pixels to the KeshOS framebuffer
- uses Chromium stub overlay/input-controller objects while native evdev event
  dispatch is implemented in the next milestone

KeshOS already exposes /dev/fb0, /dev/dri/card0 and /dev/input/event* through its
own kernel. The backend talks to those KeshOS device interfaces. It does not use
the Linux kernel.

## Important bootstrap detail

The current KeshOS C/C++ userspace toolchain deliberately uses the
`x86_64-unknown-linux-musl` ABI. For the first bring-up Chromium is therefore
built with its Linux/musl compile-time assumptions while Ozone runtime selection
is `kesh`.

That is a bootstrap bridge, not the final architecture. The next build-system
phase will add a dedicated Chromium `target_os = "keshos"` once the minimum
Chromium base/POSIX dependency set is known.

## Apply to an OpenFyde checkout

```bash
cd ports/chromium_ui
./apply_overlay.sh /path/to/openfyde/chromium
```

Then use the sample GN arguments from `args.gn.example`.

The overlay is additive. It does not overwrite Ash, Aura, Views, compositor,
Blink, V8 or the Chromium browser.
