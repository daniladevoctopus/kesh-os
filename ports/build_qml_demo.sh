#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/ports_build/qml-demo-musl"
JOBS="${JOBS:-4}"

cmake -S "$ROOT/ports/qml_demo" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/keshos-musl-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$ROOT/toolchain/musl" \
    -DQT_HOST_PATH=/usr \
    -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake
cmake --build "$BUILD" --parallel "$JOBS"
cmake -E copy "$BUILD/qml_demo" "$ROOT/ports_bin/qml_demo.elf"
