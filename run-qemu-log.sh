#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ISO="${1:-$ROOT/build/keshos.iso}"
LOG="${2:-$ROOT/serial.log}"
QEMU_LOG="${KESHOS_QEMU_LOG:-$ROOT/qemu-debug.log}"
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "qemu-system-x86_64 is required" >&2; exit 1; }
[[ -f "$ISO" ]] || { echo "ISO not found: $ISO" >&2; exit 1; }
rm -f "$LOG" "$QEMU_LOG"
echo "[KeshOS] raw kernel COM1 -> $LOG"
echo "[KeshOS] QEMU low-level trace -> $QEMU_LOG"
exec qemu-system-x86_64 \
  -machine q35,accel=tcg \
  -cpu qemu64 \
  -smp 2 \
  -m 1024 \
  -cdrom "$ISO" \
  -boot d \
  -serial "file:$LOG" \
  -d guest_errors,cpu_reset,int \
  -D "$QEMU_LOG" \
  -display gtk \
  -no-reboot \
  -no-shutdown
