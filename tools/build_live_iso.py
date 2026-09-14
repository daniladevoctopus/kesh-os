import os
import subprocess
import shutil
import stat
import struct

# 1. Prepare directories
iso_root = r'out\iso_grub_build'
full_iso_root = r'c:\Users\DanDevXP\Desktop\keshoos\out\iso_grub_build'

def remove_readonly(func, path, _):
    os.chmod(path, stat.S_IWRITE)
    func(path)

if os.path.exists(full_iso_root):
    shutil.rmtree(full_iso_root, onerror=remove_readonly)
os.makedirs(full_iso_root, exist_ok=True)

efi_extracted = r'c:\Users\DanDevXP\Desktop\keshoos\out\efi_extracted'
shutil.copytree(efi_extracted, full_iso_root, dirs_exist_ok=True)

# 2. Copy rootfs and initramfs
live_dir = os.path.join(full_iso_root, 'live')
os.makedirs(live_dir, exist_ok=True)
src_root = r'D:\openfyde\parts\2.ROOT-A.img'
dst_root = os.path.join(live_dir, 'rootfs.img')
if not os.path.exists(dst_root) or os.path.getsize(dst_root) != os.path.getsize(src_root):
    print('Copying rootfs...')
    shutil.copy2(src_root, dst_root)

src_initramfs = r'c:\Users\DanDevXP\Desktop\keshoos\out\initramfs.img'
shutil.copy2(src_initramfs, os.path.join(full_iso_root, 'syslinux', 'initramfs.img'))
os.makedirs(os.path.join(full_iso_root, 'boot'), exist_ok=True)
shutil.copy2(src_initramfs, os.path.join(full_iso_root, 'boot', 'initramfs.img'))

# 3. Create embedded auto-boot script into bootx64.efi
# GRUB image header tag 4 is embedded config
autoboot_script = b"""search --file --set=root /syslinux/vmlinuz.A
linux ($root)/syslinux/vmlinuz.A init=/init rootwait ro loglevel=7 console=tty0 earlyprintk=vga i915.modeset=1 cros_efi cros_debug
initrd ($root)/syslinux/initramfs.img
boot
\x00"""

src_bootx64 = r'c:\Users\DanDevXP\Desktop\keshoos\out\efi_extracted\efi\boot\bootx64.efi'
with open(src_bootx64, 'rb') as f:
    efi_data = bytearray(f.read())

# Find the tag prefix at offset 730425 (tag 3: prefix)
# Tag format: uint32 tag_type, uint32 tag_len (including struct header 8 bytes), followed by data
tag_pos = efi_data.find(b'\x03\x00\x00\x00\x18\x00\x00\x00/efi/boot\x00')
if tag_pos != -1:
    print(f'Found GRUB tag prefix at {tag_pos}')
    # Tag 4: embedded config
    tag_type = 4
    tag_len = 8 + len(autoboot_script)
    tag_header = struct.pack('<II', tag_type, tag_len)
    new_tag = tag_header + autoboot_script
    
    # Overwrite into efi_data (there is plenty of zero padding after prefix)
    efi_data[tag_pos:tag_pos+len(new_tag)] = new_tag
    print('Embedded 100% automated autoboot script into bootx64.efi!')

# Save patched bootx64.efi
dst_bootx64 = os.path.join(full_iso_root, 'efi', 'boot', 'bootx64.efi')
with open(dst_bootx64, 'wb') as f:
    f.write(efi_data)

# Also update in efi.img FAT partition
efi_fat = os.path.join(full_iso_root, 'efi.img')
shutil.copy2(r'D:\openfyde\parts\11.EFI-SYSTEM.img', efi_fat)

with open(efi_fat, 'r+b') as f:
    fat_data = f.read()
    # Replace bootx64.efi inside efi.img if present
    boot_pos = fat_data.find(b'/efi/boot\x00\x00\x00\x00\x00\x00\x00')
    if boot_pos != -1:
        print('Found bootx64.efi inside efi.img, updating tag...')
        # Search for tag_pos pattern inside efi.img
        idx = fat_data.find(b'\x03\x00\x00\x00\x18\x00\x00\x00/efi/boot\x00')
        if idx != -1:
            f.seek(idx)
            f.write(new_tag)
            print('Updated embedded autoboot script inside efi.img FAT!')

# 4. Build final ISO with xorriso
xorriso_bin = r'tools\xorriso\xorriso.exe'
releases_dir = r'releases'
iso_out = os.path.join(releases_dir, 'KeshOS-Beta-0.8.0-Live.iso')

print('Building native GRUB2 EFI Live ISO with autoboot xorriso...')
cmd = [
    xorriso_bin, '-as', 'mkisofs',
    '-V', 'KESHOS_LIVE',
    '-J', '-R',
    '--efi-boot', 'efi.img',
    '-efi-boot-part', '--efi-boot-image',
    '-no-emul-boot',
    '-o', iso_out,
    iso_root
]
subprocess.run(cmd, cwd=r'c:\Users\DanDevXP\Desktop\keshoos', check=True)
print('[SUCCESS] 100% AUTOMATED AUTO-BOOT KeshOS-Beta-0.8.0-Live.iso created!')
