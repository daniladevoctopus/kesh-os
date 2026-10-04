#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/ports/llvm-project-22.1.8"
BUILD="$ROOT/ports_build/llvm-musl-runtimes"
STAGE="$ROOT/ports_build/llvm-musl-stage"

cmake -S "$SRC/runtimes" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/keshos-musl-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DLLVM_PATH="$SRC/llvm" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DLIBCXX_ENABLE_SHARED=OFF \
    -DLIBCXX_ENABLE_STATIC=ON \
    -DLIBCXX_ENABLE_EXCEPTIONS=ON \
    -DLIBCXX_ENABLE_RTTI=ON \
    -DLIBCXX_ENABLE_FILESYSTEM=ON \
    -DLIBCXX_ENABLE_MONOTONIC_CLOCK=ON \
    -DLIBCXX_USE_COMPILER_RT=ON \
    -DLIBCXX_HAS_MUSL_LIBC=ON \
    -DLIBCXXABI_ENABLE_SHARED=OFF \
    -DLIBCXXABI_ENABLE_STATIC=ON \
    -DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF \
    -DLIBCXXABI_USE_COMPILER_RT=ON \
    -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
    -DLIBUNWIND_ENABLE_SHARED=OFF \
    -DLIBUNWIND_ENABLE_STATIC=ON \
    -DLIBUNWIND_USE_COMPILER_RT=ON \
    -DLIBUNWIND_ENABLE_THREADS=ON \
    -DLIBUNWIND_ENABLE_ASSERTIONS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_BENCHMARKS=OFF

cmake --build "$BUILD" --parallel
rm -rf "$STAGE"
DESTDIR="$STAGE" cmake --install "$BUILD"
cp -a "$STAGE/usr/include/." "$ROOT/toolchain/musl/include/"
cp -a "$STAGE/usr/lib/libc++.a" "$ROOT/toolchain/musl/lib/libc++.a"
cp -a "$STAGE/usr/lib/libc++abi.a" "$ROOT/toolchain/musl/lib/libc++abi.a"
cp -a "$STAGE/usr/lib/libc++experimental.a" "$ROOT/toolchain/musl/lib/libc++experimental.a"
cp -a "$STAGE/usr/lib/libunwind.a" "$ROOT/toolchain/musl/lib/libunwind.a"
