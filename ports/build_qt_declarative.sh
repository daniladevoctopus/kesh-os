#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/ports/qtdeclarative-6.11.2"
BUILD="$ROOT/ports_build/qtdeclarative-musl"
STAGE="$ROOT/ports_build/qtdeclarative-musl-stage"
JOBS="${JOBS:-4}"
export PKG_CONFIG_LIBDIR="$ROOT/toolchain/musl/lib/pkgconfig:$ROOT/toolchain/musl/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$ROOT/toolchain/musl"

cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/keshos-musl-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH="$ROOT/toolchain/musl" \
    -DQT_HOST_PATH=/usr \
    -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DQT_BUILD_BENCHMARKS=OFF \
    -DQT_BUILD_TOOLS_BY_DEFAULT=OFF \
    -DQT_BUILD_TOOLS_WHEN_CROSSCOMPILING=OFF \
    -DQT_GENERATE_SBOM=OFF \
    -DQT_FEATURE_qml_debug=OFF \
    -DQT_FEATURE_qml_network=OFF

cmake --build "$BUILD" --parallel "$JOBS"
while IFS= read -r meta; do
    name="$(basename "$meta")"
    target_name="${name#preliminary_prl_meta_info_for_}"
    target_name="${target_name%_Release.txt}"
    source_prl="$(dirname "$meta")/preliminary_prl_for_${target_name}_step2_Release.prl"
    target_prl="$(sed -n 's/^FINAL_PRL_FILE_PATH = //p' "$meta")"
    if [[ -f "$source_prl" && -n "$target_prl" ]]; then
        cmake -E copy "$source_prl" "$target_prl"
    fi
done < <(find "$BUILD" -name 'preliminary_prl_meta_info_for_*_Release.txt' -type f)
rm -rf "$STAGE"
DESTDIR="$STAGE" cmake --install "$BUILD"
cmake -E copy_directory "$STAGE/usr/include" "$ROOT/toolchain/musl/include"
cmake -E copy_directory "$STAGE/usr/lib" "$ROOT/toolchain/musl/lib"
if [[ -d "$STAGE/usr/qml" ]]; then
    cmake -E copy_directory "$STAGE/usr/qml" "$ROOT/toolchain/musl/qml"
fi
if [[ -d "$STAGE/usr/plugins" ]]; then
    cmake -E copy_directory "$STAGE/usr/plugins" "$ROOT/toolchain/musl/plugins"
fi
