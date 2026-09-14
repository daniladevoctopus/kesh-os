"""
Build clean UEFI Limine Boot Image for KeshOS Brownie.
"""
import os, shutil

boot_dir = r'c:\Users\DanDevXP\Desktop\keshoos\boot_keshos'
limine_src = r'c:\Users\DanDevXP\Desktop\keshoos\tools\limine\limine-binary'

efi_boot_dir = os.path.join(boot_dir, 'EFI', 'BOOT')
os.makedirs(efi_boot_dir, exist_ok=True)

# 1. Copy Limine UEFI binary to EFI/BOOT/BOOTX64.EFI
shutil.copyfile(os.path.join(limine_src, 'BOOTX64.EFI'), os.path.join(efi_boot_dir, 'BOOTX64.EFI'))
shutil.copyfile(os.path.join(limine_src, 'BOOTIA32.EFI'), os.path.join(efi_boot_dir, 'BOOTIA32.EFI'))

# 2. Write limine.conf
limine_conf_content = """timeout: 3
default_entry: 1
graphics: yes

/KeshOS Beta 0.8.0 "Brownie" (SneakDeak Team)
    protocol: linux
    kernel_path: boot():/EFI/BOOT/vmlinuz
    kernel_cmdline: console=tty0 root=/dev/nvme0n1p3 rw quiet loglevel=3
"""

with open(os.path.join(boot_dir, 'limine.conf'), 'w', encoding='utf-8') as f:
    f.write(limine_conf_content)

with open(os.path.join(efi_boot_dir, 'limine.conf'), 'w', encoding='utf-8') as f:
    f.write(limine_conf_content)

# 3. Copy kernel to EFI/BOOT/vmlinuz
shutil.copyfile(os.path.join(boot_dir, 'vmlinuz-keshos-6.6'), os.path.join(efi_boot_dir, 'vmlinuz'))

print(f"[OK] UEFI Limine Boot Structure configured in: {boot_dir}")
print(f"  [+] EFI/BOOT/BOOTX64.EFI")
print(f"  [+] EFI/BOOT/vmlinuz (Linux 6.6 Kernel)")
print(f"  [+] limine.conf (KeshOS Brownie)")
