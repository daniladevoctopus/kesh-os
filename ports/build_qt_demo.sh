#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/ports_build/qt-demo-musl"

cmake -S "$ROOT/ports/qt_demo" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/keshos-musl-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$ROOT/toolchain/musl"
cmake --build "$BUILD" --parallel 4
cmake -E copy "$BUILD/qt_demo" "$ROOT/ports_bin/qt_demo.elf"
