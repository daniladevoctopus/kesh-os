#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

log() { printf '\033[1;36m[KeshOS]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[KeshOS ERROR]\033[0m %s\n' "$*" >&2; exit 1; }

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install the CachyOS build dependencies first (see the README or run './build.sh --install-deps')."
}

if [[ "${1:-}" == "--install-deps" ]]; then
    command -v pacman >/dev/null 2>&1 || die "--install-deps is intended for CachyOS/Arch systems with pacman."
    sudo pacman -S --needed base-devel python ninja clang llvm lld xorriso qemu-desktop
    exit $?
fi

if [[ "$(uname -s)" != "Linux" ]]; then
    die "This build.sh is for Linux. Use build.bat/build-iso.ps1 on Windows."
fi

if [[ -n "${KESHOS_TOOLCHAIN:-}" ]]; then
    if [[ -d "$KESHOS_TOOLCHAIN/bin" ]]; then
        export KESHOS_TOOLCHAIN="$KESHOS_TOOLCHAIN/bin"
    fi
    export PATH="$KESHOS_TOOLCHAIN:$PATH"
fi

log "Checking build dependencies..."
need_cmd python3
need_cmd ninja
need_cmd clang
need_cmd clang++
need_cmd ld.lld
need_cmd llvm-ar
need_cmd xorriso

if [[ "$(uname -m)" != "x86_64" ]]; then
    die "KeshOS native build currently expects an x86_64 host toolchain."
fi

log "Generating build.ninja..."
python3 generate_ninja.py

log "Cleaning generated build output..."
rm -rf ready build

log "Building KeshOS kernel, userspace apps and packages..."
ninja -j"$(nproc)"

log "Building bootable ISO..."
ninja iso
log "Verifying ISO contains the current kernel and live installer..."
python3 tools/verify_iso.py
python3 -m py_compile tools/verify_iso.py tools/check_serial_log.py

ISO="$ROOT/build/keshos.iso"
[[ -f "$ISO" ]] || die "ISO build finished without creating $ISO"

log "Current kernel SHA256:"
sha256sum "$ROOT/build/kernel.elf"
log "ISO ready: $ISO"
ls -lh "$ISO"
