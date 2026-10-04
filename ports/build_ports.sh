#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SYSROOT="$ROOT/toolchain/musl"
OUT="$ROOT/ports_bin"
mkdir -p "$OUT"

if [[ ! -f "$SYSROOT/lib/libc.a" ]]; then
    echo "KeshOS musl sysroot is missing: $SYSROOT" >&2
    exit 2
fi

echo "Building Port 0: hello.elf..."
clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" -isystem "$SYSROOT/include" -O2 \
    "$ROOT/ports/hello_musl.c" -o "$OUT/hello.elf"

echo "Building Port 1: test_ipc.elf..."
clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" -isystem "$SYSROOT/include" -O2 \
    "$ROOT/ports/test_ipc.c" -o "$OUT/test_ipc.elf"

echo "Building Port 2: wayland_demo.elf..."
clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" -isystem "$SYSROOT/include" -O2 \
    "$ROOT/ports/wayland_demo.c" \
    "$SYSROOT/lib/libwayland-server.a" \
    "$SYSROOT/lib/libwayland-client.a" \
    "$SYSROOT/lib/libffi.a" \
    -o "$OUT/wayland_demo.elf"

echo "Building Port 3: gfx_demo.elf..."
clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" -isystem "$SYSROOT/include" \
    -isystem "$SYSROOT/include/pixman-1" -isystem "$SYSROOT/include/xkbcommon" -O2 \
    "$ROOT/ports/graphics_input_demo.c" \
    "$SYSROOT/lib/libpixman-1.a" \
    "$SYSROOT/lib/libxkbcommon.a" \
    -lm \
    -o "$OUT/gfx_demo.elf"

echo "Building Port 4: font_demo.elf..."
clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" -isystem "$SYSROOT/include" \
    -isystem "$SYSROOT/include/pixman-1" -isystem "$SYSROOT/include/freetype2" -O2 \
    "$ROOT/ports/font_render_demo.c" \
    "$SYSROOT/lib/libfreetype.a" \
    "$SYSROOT/lib/libpixman-1.a" \
    "$SYSROOT/lib/libz.a" \
    -lm \
    -o "$OUT/font_demo.elf"

echo "Building Port 5: qt_demo.elf..."
bash "$ROOT/ports/build_qt_demo.sh"

echo "Building Port 6: qml_demo.elf..."
bash "$ROOT/ports/build_qml_demo.sh"

echo "Building Port 7: wc.elf (Kesh Wayland compositor)..."
bash "$ROOT/ports/build_kesh_compositor.sh"

echo "All 8 port ELFs successfully built in $OUT:"
ls -lh "$OUT"
