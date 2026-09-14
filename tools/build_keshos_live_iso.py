"""
KESHOS OFFICIAL GENTOO LIVE ISO BUILDER
Puts console=ttyS0,115200 FIRST and console=tty0 LAST so interactive login, prompt and GUI appear directly on the main graphical screen!
"""
import os, shutil, subprocess

keshos_root = r'c:\Users\DanDevXP\Desktop\keshoos'
iso_root = os.path.join(keshos_root, 'iso_root')
dest_iso = os.path.join(keshos_root, 'releases', 'KeshOS-Beta-0.8.0-Brownie.iso')
xorriso_exe = os.path.join(keshos_root, 'tools', 'xorriso', 'xorriso.exe')
limine_bin = os.path.join(keshos_root, 'tools', 'limine', 'limine-binary')
gentoo_dir = os.path.join(keshos_root, 'gentoo_kernel')

os.makedirs(os.path.dirname(dest_iso), exist_ok=True)
shutil.rmtree(iso_root, ignore_errors=True)
os.makedirs(os.path.join(iso_root, 'EFI', 'BOOT'), exist_ok=True)
os.makedirs(os.path.join(iso_root, 'boot'), exist_ok=True)

print("1. Copying Limine Bootloader Stages...")
shutil.copyfile(os.path.join(limine_bin, 'limine-bios-cd.bin'), os.path.join(iso_root, 'limine-bios-cd.bin'))
shutil.copyfile(os.path.join(limine_bin, 'limine-uefi-cd.bin'), os.path.join(iso_root, 'limine-uefi-cd.bin'))
shutil.copyfile(os.path.join(limine_bin, 'limine-bios.sys'), os.path.join(iso_root, 'limine-bios.sys'))
shutil.copyfile(os.path.join(limine_bin, 'BOOTX64.EFI'), os.path.join(iso_root, 'EFI', 'BOOT', 'BOOTX64.EFI'))
shutil.copyfile(os.path.join(limine_bin, 'BOOTIA32.EFI'), os.path.join(iso_root, 'EFI', 'BOOT', 'BOOTIA32.EFI'))

print("2. Copying Official Gentoo Kernel and Dracut Initramfs...")
shutil.copyfile(os.path.join(gentoo_dir, 'GENTOO.'), os.path.join(iso_root, 'boot', 'vmlinuz'))
shutil.copyfile(os.path.join(gentoo_dir, 'GENTOO.IGZ'), os.path.join(iso_root, 'boot', 'initramfs.igz'))

# Copy squashfs rootfs
if os.path.exists(os.path.join(gentoo_dir, 'IMAGE.SQUASHFS')):
    print("3. Copying Gentoo Rootfs (image.squashfs)...")
    shutil.copyfile(os.path.join(gentoo_dir, 'IMAGE.SQUASHFS'), os.path.join(iso_root, 'image.squashfs'))

with open(os.path.join(iso_root, 'livecd'), 'w') as f:
    f.write('Gentoo Linux LiveCD\n')

# 3. Create limine.conf with console=tty0 as PRIMARY (last)
limine_conf = """timeout: 2
default_entry: 1
graphics: yes

/KeshOS Beta 0.8.0 "Brownie" (Gentoo Base)
    protocol: linux
    kernel_path: boot():/boot/vmlinuz
    module_path: boot():/boot/initramfs.igz
    kernel_cmdline: root=live:LABEL=KESHOS_LIVE rd.live.image rd.live.dir=/ rd.live.squashimg=image.squashfs cdroot console=ttyS0,115200 console=tty0

/KeshOS Beta 0.8.0 "Brownie" (Verbose Debug)
    protocol: linux
    kernel_path: boot():/boot/vmlinuz
    module_path: boot():/boot/initramfs.igz
    kernel_cmdline: root=live:LABEL=KESHOS_LIVE rd.live.image rd.live.dir=/ rd.live.squashimg=image.squashfs cdroot console=ttyS0,115200 console=tty0 rd.debug
"""

with open(os.path.join(iso_root, 'limine.conf'), 'w', encoding='utf-8') as f:
    f.write(limine_conf)
with open(os.path.join(iso_root, 'EFI', 'BOOT', 'limine.conf'), 'w', encoding='utf-8') as f:
    f.write(limine_conf)

# 4. Generate ISO with Volume Label KESHOS_LIVE
print("4. Generating Bootable Live ISO with Volume Label KESHOS_LIVE...")
cmd = [
    xorriso_exe,
    '-as', 'mkisofs',
    '-V', 'KESHOS_LIVE',
    '-b', 'limine-bios-cd.bin',
    '-no-emul-boot',
    '-boot-load-size', '4',
    '-boot-info-table',
    '--efi-boot', 'limine-uefi-cd.bin',
    '-efi-boot-part',
    '--efi-boot-image',
    '--protective-msdos-label',
    'iso_root',
    '-o', 'releases/KeshOS-Beta-0.8.0-Brownie.iso'
]

subprocess.run(cmd, cwd=keshos_root, check=True)
print(f"[OK] ISO Generated: {dest_iso} ({os.path.getsize(dest_iso) / (1024*1024):.2f} MB)")

# 5. Run limine bios-install
limine_tool = os.path.join(limine_bin, 'limine-tool-windows-x86', 'limine.exe')
if os.path.exists(limine_tool):
    print("5. Installing Limine BIOS stages into ISO...")
    subprocess.run([limine_tool, 'bios-install', dest_iso], check=True)

print("==================================================================")
print("  [SUCCESS] KESHOS LIVE ISO BUILT WITH TTY0 DISPLAY FOCUS!       ")
print("==================================================================")
