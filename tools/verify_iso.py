#!/usr/bin/env python3
from pathlib import Path
import hashlib
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
iso = root / "build" / "keshos.iso"
kernel = root / "build" / "kernel.elf"
installer = root / "build" / "apps" / "installer.kea"

if not iso.exists() or not kernel.exists() or not installer.exists():
    raise SystemExit("verify_iso: missing build artifact or build/keshos.iso")
xorriso = shutil.which("xorriso")
if not xorriso:
    raise SystemExit("verify_iso: xorriso not found")

with tempfile.TemporaryDirectory(prefix="keshos-iso-check-") as td:
    checks = (("kernel", kernel, "/boot/kernel.elf"), ("installer", installer, "/apps/installer.kea"))
    for label, source, iso_path in checks:
        out = Path(td) / source.name
        cmd = [xorriso, "-osirrox", "on", "-indev", str(iso), "-extract", iso_path, str(out)]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if res.returncode != 0 or not out.exists():
            print(res.stdout, end="")
            raise SystemExit(f"verify_iso: failed to extract {iso_path} from ISO")
        a = hashlib.sha256(source.read_bytes()).hexdigest()
        b = hashlib.sha256(out.read_bytes()).hexdigest()
        print(f"build {label} sha256: {a}")
        print(f"ISO   {label} sha256: {b}")
        if a != b:
            raise SystemExit(f"verify_iso: ISO contains a DIFFERENT {source.name} (stale ISO)")
print("verify_iso: PASS (ISO contains current kernel and installer)")
