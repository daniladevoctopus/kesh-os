# Tomorrow: first OzoneKesh frame on KeshOS

The goal is deliberately smaller than full Ash: prove that real Chromium/OpenFyde
native code can create a platform window, rasterize with Skia, run Chromium's
UI message loop and place pixels on the KeshOS framebuffer.

## 1. Update the port branch

```bash
git fetch origin
git switch port/openfyde-ozone-kesh
git pull --ff-only
```

Do not merge this branch into `main` before the smoke test works.

## 2. Install host tools on CachyOS / Arch

```bash
sudo pacman -S --needed base-devel git python ninja clang lld llvm cmake xorriso
```

Make sure the disk containing the OpenFyde workspace has plenty of free space.
Chromium DEPS are large even though we only build a small target.

## 3. Run the one-command bring-up

From the KeshOS repository:

```bash
./ports/chromium_ui/tomorrow_build.sh
```

For the current 2-core / 4-thread development machine, leave the default two
Chromium build jobs. If you want to choose a faster disk:

```bash
OPENFYDE_WORKSPACE=/path/on/fast/disk/openfyde-kesh-work \
KESH_CHROMIUM_JOBS=2 \
./ports/chromium_ui/tomorrow_build.sh
```

The script will:

1. initialize the pinned BearSSL source needed by KeshOS;
2. obtain the pinned OpenFyde Chromium + DEPS using depot_tools/gclient;
3. apply the OzoneKesh overlay;
4. cross-build `//keshos/ozone:kesh_smoke` as a static KeshOS ET_EXEC;
5. reject PT_INTERP, PT_DYNAMIC or W+X output;
6. stage it as `ports_bin/kesh_smoke.elf`;
7. build `build/keshos.iso`.

The first Chromium source/DEPS sync can take substantially longer than the
actual KeshOS kernel build. Re-running reuses the checkout.

## 4. Boot and run

Boot:

```text
build/keshos.iso
```

Open KeshOS Terminal and run:

```text
run /boot/apps/kesh_smoke.elf
```

Expected first visual result:

- dark 900x560 Chromium/Skia surface;
- dark top strip and launcher-style rail;
- three grey content cards;
- orange accent block;
- green heartbeat bar moving along the bottom.

The moving green bar matters: if it moves, Chromium `base::MessagePump`, timer,
Skia, OzoneKesh, framebuffer mmap and the KeshOS scheduler are all participating
in the same running path.

## 5. If it fails

Do not start changing random kernel code. Save:

- the last 150-250 lines of the build output;
- `serial.log` from KeshOS/QEMU if the ELF starts and crashes;
- the exact last KLOG line;
- whether normal KeshOS still reached its desktop before launching the test.

Compiler failure = fix the port/toolchain first.
ELF loader failure = inspect ET_EXEC/segments/syscalls.
Runtime page fault = inspect the logged RIP/CR2 and framebuffer mapping.
Blank test surface with a live OS = inspect Ozone initialization/presentation.

## What this is not yet

This is not Ash, Shelf or Launcher yet. It is the smallest real Chromium-native
foundation underneath those layers. After it works, the next sequence is:

```text
kesh_smoke -> upstream ozone_demo -> Aura -> Views -> Ash WM -> Shelf/Launcher
```
