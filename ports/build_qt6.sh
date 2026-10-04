#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/ports/qtbase-6.11.2"
BUILD="$ROOT/ports_build/qt6-musl"
STAGE="$ROOT/ports_build/qt6-musl-stage"
JOBS="${JOBS:-4}"
export PKG_CONFIG_LIBDIR="$ROOT/toolchain/musl/lib/pkgconfig:$ROOT/toolchain/musl/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$ROOT/toolchain/musl"

cmake -E make_directory "$BUILD"
cd "$BUILD"
"$SRC/configure" \
    -prefix /usr \
    -release \
    -static \
    -opensource \
    -confirm-license \
    -nomake examples \
    -nomake tests \
    -no-dbus \
    -no-opengl \
    -no-openssl \
    -no-feature-glib \
    -no-feature-icu \
    -no-feature-cups \
    -no-feature-fontconfig \
    -no-feature-gtk3 \
    -no-feature-libudev \
    -no-feature-libinput \
    -no-feature-mtdev \
    -no-feature-tslib \
    -no-feature-xcb \
    -no-feature-wayland \
    -no-feature-waylandscanner \
    -no-feature-egl \
    -no-feature-eglfs \
    -no-feature-gbm \
    -no-feature-kms \
    -feature-linuxfb \
    -feature-evdev \
    -qpa linuxfb \
    -default-qpa linuxfb \
    -qt-doubleconversion \
    -qt-pcre \
    -qt-zlib \
    -qt-freetype \
    -qt-harfbuzz \
    -qt-libpng \
    -qt-libjpeg \
    -qt-host-path /usr \
    -- \
    -DCMAKE_TOOLCHAIN_FILE=../../ports/keshos-musl-toolchain.cmake \
    -DQT_BUILD_TOOLS_BY_DEFAULT=OFF \
    -DQT_BUILD_BENCHMARKS=OFF \
    -DQT_BUILD_MANUAL_TESTS=OFF \
    -DQT_BUILD_MINIMAL_STATIC_TESTS=OFF

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
cmake -E copy_directory "$STAGE/usr/plugins" "$ROOT/toolchain/musl/plugins"
cmake -E copy_directory "$STAGE/usr/mkspecs" "$ROOT/toolchain/musl/mkspecs"
cmake -E copy_directory "$STAGE/usr/metatypes" "$ROOT/toolchain/musl/metatypes"
