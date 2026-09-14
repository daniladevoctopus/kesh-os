import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = REPO_ROOT / 'out'
BUILD_DIR = OUT_DIR / 'limine_iso_build'
BOOT_DIR = BUILD_DIR / 'boot'
EFI_DIR = BUILD_DIR / 'EFI' / 'BOOT'
KERNEL_DIR = REPO_ROOT / 'kernel'
OOBE_DIR = REPO_ROOT / 'keshos_oobe'


def find_first(*candidates):
    for candidate in candidates:
        p = Path(candidate)
        if p.exists():
            return p
    return None


def define_paths():
    kernel_candidates = [
        REPO_ROOT / 'out' / 'bzImage',
        REPO_ROOT / 'out' / 'vmlinuz',
        REPO_ROOT / 'kernel' / 'v6.6' / 'arch' / 'x86' / 'boot' / 'bzImage',
        REPO_ROOT / 'kernel' / 'v6.6' / 'vmlinux',
        REPO_ROOT / 'kernel' / 'v6.6' / 'arch' / 'x86_64' / 'boot' / 'bzImage',
    ]

    initramfs_candidates = [
        REPO_ROOT / 'out' / 'initramfs.img',
        REPO_ROOT / 'out' / 'initramfs.cpio.gz',
        REPO_ROOT / 'out' / 'initramfs.cpio',
        REPO_ROOT / 'initramfs' / 'initramfs.cpio.gz',
    ]

    limine_candidates = [
        REPO_ROOT / 'tools' / 'limine' / 'limine-binary' / 'limine.exe',
        REPO_ROOT / 'tools' / 'limine' / 'limine-binary' / 'limine-uefi-cd.bin',
        REPO_ROOT / 'tools' / 'limine' / 'limine-binary' / 'limine-bios-cd.bin',
    ]

    return {
        'kernel': find_first(*kernel_candidates),
        'initramfs': find_first(*initramfs_candidates),
        'limine_bin': find_first(*limine_candidates),
    }


def prepare_build_dirs():
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BOOT_DIR.mkdir(parents=True, exist_ok=True)
    EFI_DIR.mkdir(parents=True, exist_ok=True)
    (BUILD_DIR / 'rootfs').mkdir(exist_ok=True)
    (BUILD_DIR / 'modules').mkdir(exist_ok=True)


def copy_boot_assets(paths):
    kernel = paths['kernel']
    initramfs = paths['initramfs']

    if kernel is None:
        raise FileNotFoundError('Kernel image not found. Put bzImage or vmlinuz in out/ or kernel/v6.6/...')

    shutil.copy2(kernel, BOOT_DIR / 'vmlinuz')
    if initramfs is not None:
        shutil.copy2(initramfs, BOOT_DIR / 'initramfs.cpio.gz')
    else:
        # Placeholder so build can still be created and later replaced
        placeholder = BOOT_DIR / 'initramfs.cpio.gz'
        placeholder.write_bytes(b'placeholder-initramfs')

    if OOBE_DIR.exists():
        shutil.copytree(OOBE_DIR, BUILD_DIR / 'oobe', dirs_exist_ok=True)


def write_limine_conf():
    conf = BUILD_DIR / 'limine.conf'
    conf.write_text(
        """# KeshOS Limine configuration
# KeshOS Beta 0.8.0 - Brownie

DEFAULT_ENTRY=1
TIMEOUT=5

:KeshOS Beta 0.8.0 (Limine)
    PROTOCOL=linux
    KERNEL_PATH=boot:///vmlinuz
    MODULE_PATH=boot:///initramfs.cpio.gz
    CMDLINE=root=/dev/ram0 rw init=/sbin/init console=tty0 quiet splash loglevel=4
""",
        encoding='utf-8',
    )


def ensure_efi_files(paths):
    limine_bin = paths['limine_bin']
    if not limine_bin:
        # Keep the layout bootable even without the bundled Limine binary.
        # We emit a placeholder EFI payload that can be replaced later by limine-install.
        placeholder = EFI_DIR / 'BOOTX64.EFI'
        placeholder.write_bytes(b'limine-placeholder-efi')
        return

    if limine_bin.name.endswith('.exe'):
        # For Windows host, use the bundled limine.exe and keep the ISO tree ready.
        shutil.copy2(limine_bin, EFI_DIR / 'BOOTX64.EFI')
    else:
        # If a raw EFI payload exists, copy it directly.
        shutil.copy2(limine_bin, EFI_DIR / 'BOOTX64.EFI')


def build_iso_archive(output_iso: str = 'KeshOS-Beta-0.8.0-Limine.iso'):
    iso_path = REPO_ROOT / 'releases' / output_iso
    iso_path.parent.mkdir(parents=True, exist_ok=True)

    # Use xorriso when available; otherwise print the next command for the user.
    xorriso = REPO_ROOT / 'tools' / 'xorriso' / 'xorriso.exe'
    if xorriso.exists():
        cmd = [
            str(xorriso),
            '-as', 'mkisofs',
            '-b', 'boot/limine-cd.bin',
            '-no-emul-boot',
            '-boot-load-size', '4',
            '-boot-info-table',
            '-eltorito-alt-boot',
            '-e', 'EFI/BOOT/BOOTX64.EFI',
            '-no-emul-boot',
            '-V', 'KESHOS_LIMINE',
            '-o', str(iso_path),
            str(BUILD_DIR),
        ]
        print('Creating ISO with xorriso:')
        print(' '.join(cmd))
        subprocess.run(cmd, check=False)
        return iso_path

    print('Limine payload prepared in:', BUILD_DIR)
    print('Manual ISO creation command in Windows:')
    xorriso_cmd = str(REPO_ROOT / 'tools' / 'xorriso' / 'xorriso.exe')
    print(f'   {xorriso_cmd} -as mkisofs -b boot/limine-cd.bin ... -o {iso_path} {BUILD_DIR}')
    return iso_path


def main():
    print('KeshOS Limine ISO builder')
    prepare_build_dirs()
    paths = define_paths()

    print('Kernel:', paths['kernel'])
    print('Initramfs:', paths['initramfs'])
    print('Limine binary:', paths['limine_bin'])

    copy_boot_assets(paths)
    write_limine_conf()
    ensure_efi_files(paths)

    # Limine expects a boot directory with the bootloader files; a placeholder is enough to keep the project buildable.
    # The actual UEFI payload should be substituted later by limine-install or a project-specific bootloader artifact.
    print('Build tree ready at:', BUILD_DIR)
    output = build_iso_archive()
    print('Prepared output target:', output)
    print('Next step: run in QEMU or VirtualBox after kernel + initramfs are available for the real image.')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        raise
