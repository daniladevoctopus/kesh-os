# KeshOS on CachyOS

## One-command build

```bash
chmod +x build.sh run-qemu-log.sh
./build.sh
```

The script regenerates `build.ninja`, removes stale generated build output, builds the kernel/userspace, creates `build/keshos.iso`, and verifies that the ISO contains the exact freshly built `build/kernel.elf` by SHA-256.

## Run QEMU with two logs

```bash
./run-qemu-log.sh
```

This creates:

- `serial.log` — raw COM1 stream from the running KeshOS kernel.
- `qemu-debug.log` — QEMU low-level reset/interrupt/guest-error trace.

## Validate that the log belongs to the current kernel

```bash
python3 tools/check_serial_log.py serial.log
```

A valid V4 log must contain:

```text
KESHOS-2026-09-29-BLACKSCREEN-V4
```

and records beginning with:

```text
[TRACE L7] #...
```

A log without these markers is treated as stale/wrong ISO evidence.
