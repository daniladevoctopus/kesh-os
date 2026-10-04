#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/ports_build/kesh-shell-musl"
JOBS="${JOBS:-$(nproc)}"

mkdir -p "$ROOT/ports_bin"
cmake -S "$ROOT/ports/kesh_shell" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/keshos-musl-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$ROOT/toolchain/musl" \
    -DQT_HOST_PATH=/usr \
    -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake
cmake --build "$BUILD" --parallel "$JOBS"
cmake -E copy "$BUILD/kesh_shell" "$ROOT/ports_bin/shell.elf"
echo "[OK] Built $ROOT/ports_bin/shell.elf"
