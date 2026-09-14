"""
KeshOS Branding & Identity Injection Engine.
Prepares all official KeshOS configuration files, branding assets, wallpaper, and system release descriptors.
"""
import os, shutil

keshos_root = r'c:\Users\DanDevXP\Desktop\keshoos'
branding_dir = os.path.join(keshos_root, 'branding')
os.makedirs(os.path.join(branding_dir, 'etc'), exist_ok=True)
os.makedirs(os.path.join(branding_dir, 'usr', 'share', 'pixmaps'), exist_ok=True)
os.makedirs(os.path.join(branding_dir, 'usr', 'share', 'wallpapers', 'KeshOS_Brownie'), exist_ok=True)
os.makedirs(os.path.join(branding_dir, 'usr', 'share', 'plasma', 'look-and-feel', 'org.keshos.brownie.desktop'), exist_ok=True)

# 1. /etc/os-release
os_release = """NAME="KeshOS"
PRETTY_NAME="KeshOS Beta 0.8.0 «Brownie»"
ID=keshos
ID_LIKE="gentoo"
VERSION="0.8.0 Beta"
VERSION_ID="0.8.0"
VERSION_CODENAME="Brownie"
HOME_URL="https://keshos.org"
SUPPORT_URL="https://keshos.org/support"
BUG_REPORT_URL="https://keshos.org/bugs"
LOGO=keshos-logo
ANSI_COLOR="0;33"
"""
with open(os.path.join(branding_dir, 'etc', 'os-release'), 'w', encoding='utf-8') as f:
    f.write(os_release)

# 2. /etc/lsb-release
lsb_release = """DISTRIB_ID=KeshOS
DISTRIB_RELEASE=0.8.0
DISTRIB_CODENAME=Brownie
DISTRIB_DESCRIPTION="KeshOS Beta 0.8.0 «Brownie»"
"""
with open(os.path.join(branding_dir, 'etc', 'lsb-release'), 'w', encoding='utf-8') as f:
    f.write(lsb_release)

# 3. /etc/issue & /etc/issue.net
issue = """
 \e[1;33m🍫 KeshOS Beta 0.8.0 "Brownie"\e[0m (KDE Plasma on Wayland)
 SneakDeak Team • Linux \r (\m) • Domain: @kesh.local

"""
with open(os.path.join(branding_dir, 'etc', 'issue'), 'w', encoding='utf-8') as f:
    f.write(issue)
with open(os.path.join(branding_dir, 'etc', 'issue.net'), 'w', encoding='utf-8') as f:
    f.write(issue)

# 4. /etc/hostname
with open(os.path.join(branding_dir, 'etc', 'hostname'), 'w', encoding='utf-8') as f:
    f.write("keshos\n")

# 5. Copy official Logos to Pixmaps
logo_src = os.path.join(keshos_root, 'keshos_logo.png')
if os.path.exists(logo_src):
    shutil.copyfile(logo_src, os.path.join(branding_dir, 'usr', 'share', 'pixmaps', 'keshos-logo.png'))
    shutil.copyfile(logo_src, os.path.join(branding_dir, 'usr', 'share', 'pixmaps', 'distributor-logo-keshos.png'))

print("[SUCCESS] All KeshOS branding files and configurations generated in:", branding_dir)
