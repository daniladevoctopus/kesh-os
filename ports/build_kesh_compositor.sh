#!/usr/bin/env bash
set -euo pipefail

ROOT="/home/danila/Рабочий стол/keshos"
SYSROOT="$ROOT/toolchain/musl"
SRC="$ROOT/ports/kesh_compositor"
# Keep the on-disc name ISO9660-safe.  KeshOS currently reads primary ISO9660
# directory names and does not consume Rock Ridge long-name records.
OUT="$ROOT/ports_bin/wc.elf"
XML="$ROOT/ports/qtbase-6.11.2/src/3rdparty/wayland/protocols/xdg-shell/xdg-shell.xml"

mkdir -p "$SRC/protocol" "$ROOT/ports_bin"
wayland-scanner server-header "$XML" "$SRC/protocol/xdg-shell-server-protocol.h"
wayland-scanner client-header "$XML" "$SRC/protocol/xdg-shell-client-protocol.h"
wayland-scanner private-code "$XML" "$SRC/protocol/xdg-shell-protocol.c"

clang --target=x86_64-unknown-linux-musl --sysroot="$SYSROOT" \
    -isystem "$SYSROOT/include" -I"$SRC" -O2 -Wall -Wextra \
    -static -fno-pie -Wl,-no-pie \
    "$SRC/main.c" "$SRC/client.c" "$SRC/protocol/xdg-shell-protocol.c" \
    "$SYSROOT/lib/libwayland-server.a" "$SYSROOT/lib/libwayland-client.a" \
    "$SYSROOT/lib/libffi.a" \
    -o "$OUT"

echo "Built $OUT"
