from pathlib import Path
import shutil
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
READY = ROOT / 'ready'
BUILD = ROOT / 'build'


def clean_stage():
    if READY.exists():
        shutil.rmtree(READY)
    dirs = [
        READY / 'boot' / 'limine',
        READY / 'boot' / 'apps',
        READY / 'boot' / 'fonts',
        READY / 'apps',
        READY / 'EFI' / 'BOOT',
        READY / 'system',
        READY / 'icons'
    ]
    for d in dirs:
        d.mkdir(parents=True, exist_ok=True)

    copies = [
        (BUILD / 'kernel.elf', READY / 'boot' / 'kernel.elf'),
        (BUILD / 'apps' / 'notepad.elf', READY / 'boot' / 'apps' / 'notepad.elf'),
        (BUILD / 'apps' / 'explorer.elf', READY / 'boot' / 'apps' / 'explorer.elf'),
        (BUILD / 'apps' / 'settings.elf', READY / 'boot' / 'apps' / 'settings.elf'),
        (BUILD / 'apps' / 'about.elf', READY / 'boot' / 'apps' / 'about.elf'),
        (BUILD / 'apps' / 'notepad.kea', READY / 'apps' / 'notepad.kea'),
        (BUILD / 'apps' / 'explorer.kea', READY / 'apps' / 'explorer.kea'),
        (BUILD / 'apps' / 'taskmgr.kea', READY / 'apps' / 'taskmgr.kea'),
        (BUILD / 'apps' / 'paint.kea', READY / 'apps' / 'paint.kea'),
        (BUILD / 'apps' / 'shell.kea', READY / 'apps' / 'shell.kea'),
        (BUILD / 'apps' / 'settings.kea', READY / 'apps' / 'settings.kea'),
        (BUILD / 'apps' / 'about.kea', READY / 'apps' / 'about.kea'),
        (BUILD / 'apps' / 'browser.kea', READY / 'apps' / 'browser.kea'),
        (BUILD / 'apps' / 'installer.kea', READY / 'apps' / 'installer.kea'),
        (ROOT / 'boot' / 'limine' / 'limine-bios-cd.bin', READY / 'boot' / 'limine' / 'limine-bios-cd.bin'),
        (ROOT / 'boot' / 'limine' / 'limine-uefi-cd.bin', READY / 'boot' / 'limine' / 'limine-uefi-cd.bin'),
        (ROOT / 'boot' / 'limine' / 'limine-bios.sys', READY / 'boot' / 'limine' / 'limine-bios.sys'),
        (ROOT / 'boot' / 'limine' / 'BOOTX64.EFI', READY / 'EFI' / 'BOOT' / 'BOOTX64.EFI'),
        (ROOT / 'boot' / 'limine' / 'limine.conf', READY / 'limine.conf'),
        (ROOT / 'boot' / 'limine' / 'limine.cfg', READY / 'limine.cfg'),
        (ROOT / 'boot' / 'limine' / 'limine.conf', READY / 'boot' / 'limine' / 'limine.conf'),
        (ROOT / 'boot' / 'limine' / 'limine.cfg', READY / 'boot' / 'limine' / 'limine.cfg'),
        (ROOT / 'boot' / 'limine' / 'limine.conf', READY / 'EFI' / 'BOOT' / 'limine.conf'),
        (ROOT / 'boot' / 'limine' / 'limine.cfg', READY / 'EFI' / 'BOOT' / 'limine.cfg'),
        (ROOT / 'boot' / 'wm.conf', READY / 'wm.conf'),
        (ROOT / 'boot' / 'wm.conf', READY / 'system' / 'wm.conf'),
    ]
    for src, dst in copies:
        if src.exists():
            shutil.copy2(src, dst)
        elif 'browser' not in src.name:
            raise RuntimeError(f'missing build artifact: {src}')

    icons = ROOT / 'assets' / 'icons'
    for icon in icons.glob('*.ico'):
        shutil.copy2(icon, READY / 'icons' / icon.name)

    ports_bin = ROOT / 'ports_bin'
    if ports_bin.exists():
        for elf in ports_bin.glob('*.elf'):
            shutil.copy2(elf, READY / 'boot' / 'apps' / elf.name)

    fonts = ROOT / 'assets' / 'fonts'
    if fonts.exists():
        for fnt in fonts.glob('*'):
            if fnt.is_file():
                shutil.copy2(fnt, READY / 'boot' / 'fonts' / fnt.name)


def find_xorriso():
    env_toolchain = os.environ.get("KESHOS_TOOLCHAIN")
    candidates = []
    if env_toolchain:
        candidates += [Path(env_toolchain) / "xorriso", Path(env_toolchain) / "xorriso.exe"]
    candidates += [Path("D:/keshos-toolchain/KeshBE/bin/xorriso.exe")]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    found = shutil.which("xorriso") or shutil.which("xorriso.exe")
    if found:
        return found
    raise RuntimeError("xorriso not found. Install xorriso with your Linux package manager.")


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    clean_stage()
    xorriso = find_xorriso()
    out_iso = BUILD / 'keshos.iso'

    cmd = [
        xorriso,
        "-as", "mkisofs",
        "-b", "boot/limine/limine-bios-cd.bin",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "--efi-boot", "boot/limine/limine-uefi-cd.bin",
        "-efi-boot-part",
        "--efi-boot-image",
        "--protective-msdos-label",
        "ready",
        "-o", "build/keshos.iso"
    ]

    res = subprocess.run(cmd, cwd=str(ROOT))
    if res.returncode != 0:
        sys.exit(res.returncode)

    print(f"KeshOS ISO created successfully: {out_iso}")


if __name__ == '__main__':
    main()
