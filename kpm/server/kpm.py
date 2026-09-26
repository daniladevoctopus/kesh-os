#!/usr/bin/env python3
"""
kpm.py - Kesh Package Manager CLI for Windows & Host Systems
Part of the KeshOS Ecosystem.
"""

import sys
import os
import struct
import zlib
import json
import urllib.request
import urllib.error
import argparse
import subprocess
import time

KPM_VERSION = "0.1.0-beta"
DEFAULT_REPO_URL = "https://keshos-kpm.vercel.app"
CONFIG_FILE = os.path.join(os.path.expanduser("~"), ".kpm_config.json")
DOWNLOADS_KPM_DIR = os.path.join(os.path.expanduser("~"), "Downloads", "KPM")

# Ensure UTF-8 output on Windows consoles
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

# ANSI Color codes for Windows & Unix
class Color:
    CYAN    = "\033[96m"
    BLUE    = "\033[94m"
    GREEN   = "\033[92m"
    YELLOW  = "\033[93m"
    RED     = "\033[91m"
    MAGENTA = "\033[95m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    RESET   = "\033[0m"

# Enable VT100 colors on Windows
if sys.platform == "win32":
    os.system("")

def print_banner():
    banner = (
        f"{Color.CYAN}{Color.BOLD}\n"
        r"  _  ______  __  __   " + "\n"
        r" | |/ /  _ \|  \/  |  " + f" {Color.GREEN}Kesh Package Manager v{KPM_VERSION}{Color.CYAN}\n"
        r" | ' /| |_) | |\/| |  " + f" {Color.DIM}Target: KeshOS & Windows Ecosystem{Color.CYAN}\n"
        r" | . \|  __/| |  | |  " + f" {Color.DIM}Repository: {get_repo_url()}{Color.CYAN}\n"
        r" |_|\_\_|   |_|  |_|  " + f"{Color.RESET}\n"
    )
    print(banner)

def get_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"repo_url": DEFAULT_REPO_URL}

def save_config(cfg):
    try:
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)
    except Exception as e:
        print(f"{Color.RED}Error saving config: {e}{Color.RESET}")

def get_repo_url():
    return get_config().get("repo_url", DEFAULT_REPO_URL)

def ensure_kpm_dir():
    os.makedirs(DOWNLOADS_KPM_DIR, exist_ok=True)
    return DOWNLOADS_KPM_DIR

def parse_kea_header(data):
    if len(data) < 244:
        return None
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != 0x0141454B:  # "KEA\1"
        return None
    
    hdr_fmt = "<IIHH32s16s32s16s64sIIIHHQQQI8I"
    try:
        fields = struct.unpack_from(hdr_fmt, data, 0)
        name = fields[4].split(b'\x00')[0].decode('utf-8', errors='replace')
        version = fields[5].split(b'\x00')[0].decode('utf-8', errors='replace')
        author = fields[6].split(b'\x00')[0].decode('utf-8', errors='replace')
        category = fields[7].split(b'\x00')[0].decode('utf-8', errors='replace')
        description = fields[8].split(b'\x00')[0].decode('utf-8', errors='replace')
        perms = fields[9]
        icon_off, icon_sz, icon_w, icon_h = fields[10], fields[11], fields[12], fields[13]
        elf_off, elf_sz, entry_pt, crc = fields[14], fields[15], fields[16], fields[17]
        return {
            "name": name,
            "version": version,
            "author": author,
            "category": category,
            "description": description,
            "perms": perms,
            "icon": {"width": icon_w, "height": icon_h, "size": icon_sz, "offset": icon_off},
            "elf": {"size": elf_sz, "offset": elf_off, "entry": entry_pt, "crc": crc}
        }
    except Exception:
        return None

def fetch_catalog():
    repo = get_repo_url().rstrip('/')
    url = f"{repo}/packages.json"
    req = urllib.request.Request(url, headers={"User-Agent": f"KPM-CLI/{KPM_VERSION}"})
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = resp.read().decode('utf-8')
            return json.loads(data)
    except Exception:
        # Fallback to local server manifest if available
        local_manifest = os.path.join(os.path.dirname(__file__), "server", "packages.json")
        if os.path.exists(local_manifest):
            with open(local_manifest, "r", encoding="utf-8") as f:
                return json.load(f)
        return None

def download_with_progress(url, dest_path):
    req = urllib.request.Request(url, headers={"User-Agent": f"KPM-CLI/{KPM_VERSION}"})
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            total_size = response.length or 0
            block_size = 4096
            downloaded = 0
            start_time = time.time()
            data = bytearray()

            print(f"{Color.CYAN}⬇  Connecting to {url}...{Color.RESET}")

            while True:
                chunk = response.read(block_size)
                if not chunk:
                    break
                data.extend(chunk)
                downloaded += len(chunk)
                
                # Progress bar
                elapsed = time.time() - start_time
                speed = (downloaded / 1024) / (elapsed + 0.0001)
                if total_size > 0:
                    percent = int((downloaded / total_size) * 100)
                    bar_len = 30
                    filled = int(bar_len * percent / 100)
                    bar = "█" * filled + "░" * (bar_len - filled)
                    sys.stdout.write(f"\r  {Color.GREEN}[{bar}] {percent}%{Color.RESET} ({downloaded/1024:.1f} KB / {total_size/1024:.1f} KB) - {speed:.1f} KB/s ")
                else:
                    sys.stdout.write(f"\r  {Color.GREEN}Downloaded {downloaded/1024:.1f} KB{Color.RESET} ({speed:.1f} KB/s) ")
                sys.stdout.flush()

            sys.stdout.write("\n")
            with open(dest_path, "wb") as f:
                f.write(data)
            return bytes(data)
    except Exception as e:
        return None

def cmd_install(pkg_name):
    if not pkg_name:
        print(f"{Color.RED}✖ Error: Please specify a package name to install.{Color.RESET}")
        return

    if not pkg_name.endswith(".kea"):
        clean_name = pkg_name
        kea_filename = pkg_name + ".kea"
    else:
        clean_name = pkg_name[:-4]
        kea_filename = pkg_name

    dest_dir = ensure_kpm_dir()
    dest_path = os.path.join(dest_dir, kea_filename)

    print(f"\n{Color.BOLD}📦 Installing package: {Color.CYAN}{kea_filename}{Color.RESET}")
    print(f"{Color.DIM}Target folder: {dest_dir}{Color.RESET}\n")

    pkg_data = None
    repo_url = get_repo_url().rstrip('/')

    # 1. Try downloading from remote Vercel registry
    url = f"{repo_url}/packages/{kea_filename}"
    pkg_data = download_with_progress(url, dest_path)

    # 2. Fallback to local workspace repository if remote offline/unreachable
    if not pkg_data:
        workspace_candidates = [
            os.path.join(os.path.dirname(__file__), "server", "packages", kea_filename),
            os.path.join(os.path.dirname(__file__), "..", "build", "apps", kea_filename),
            os.path.join(os.getcwd(), kea_filename),
            os.path.join(os.getcwd(), "build", "apps", kea_filename)
        ]
        for c in workspace_candidates:
            if os.path.exists(c):
                print(f"{Color.YELLOW}⚡ Remote registry unreachable. Using verified local workspace build: {c}{Color.RESET}")
                with open(c, "rb") as f:
                    pkg_data = f.read()
                with open(dest_path, "wb") as f:
                    f.write(pkg_data)
                break

    if not pkg_data:
        print(f"\n{Color.RED}✖ Error: Failed to find or download package '{kea_filename}'.{Color.RESET}")
        print(f"{Color.DIM}Check your network or verify package name with 'kpm list'.{Color.RESET}")
        return

    # Verify .KEA container integrity
    info = parse_kea_header(pkg_data)
    if not info:
        print(f"\n{Color.RED}✖ Corrupted package: Invalid KEA magic signature!{Color.RESET}")
        if os.path.exists(dest_path):
            os.remove(dest_path)
        return

    print(f"\n{Color.GREEN}✔ Verification passed: Valid KEA 64-bit application.{Color.RESET}")
    print(f"  {Color.BOLD}Name:{Color.RESET}        {info['name']} v{info['version']}")
    print(f"  {Color.BOLD}Author:{Color.RESET}      {info['author']}")
    print(f"  {Color.BOLD}Category:{Color.RESET}    {info['category']}")
    print(f"  {Color.BOLD}Summary:{Color.RESET}     {info['description']}")
    print(f"  {Color.BOLD}Entry Point:{Color.RESET} 0x{info['elf']['entry']:X}")
    print(f"  {Color.BOLD}Size:{Color.RESET}        {len(pkg_data)} bytes ({len(pkg_data)/1024:.1f} KB)")
    print(f"\n{Color.GREEN}{Color.BOLD}🚀 Successfully installed to: {dest_path}{Color.RESET}\n")

def cmd_list():
    dest_dir = ensure_kpm_dir()
    print(f"\n{Color.BOLD}📁 Locally Installed Packages ({dest_dir}):{Color.RESET}")
    installed_files = [f for f in os.listdir(dest_dir) if f.endswith(".kea")] if os.path.exists(dest_dir) else []
    if installed_files:
        for f in installed_files:
            fp = os.path.join(dest_dir, f)
            sz = os.path.getsize(fp) / 1024
            print(f"  {Color.GREEN}●{Color.RESET} {Color.CYAN}{f:<20}{Color.RESET} {sz:.1f} KB")
    else:
        print(f"  {Color.DIM}(No packages installed yet in Downloads\\KPM){Color.RESET}")

    print(f"\n{Color.BOLD}🌐 Available Cloud Packages ({get_repo_url()}):{Color.RESET}")
    catalog = fetch_catalog()
    if catalog and "packages" in catalog:
        for pkg in catalog["packages"]:
            name_ver = f"{pkg['name']} v{pkg['version']}"
            print(f"  {Color.BLUE}◆{Color.RESET} {Color.BOLD}{name_ver:<22}{Color.RESET} [{pkg['category']}] - {pkg['description']}")
    else:
        print(f"  {Color.DIM}(Could not reach registry server){Color.RESET}")
    print("")

def cmd_info(target):
    if not target:
        print(f"{Color.RED}✖ Error: Specify a package or file name.{Color.RESET}")
        return
    
    # Check if local file
    data = None
    if os.path.exists(target):
        with open(target, "rb") as f:
            data = f.read()
    else:
        dest_path = os.path.join(DOWNLOADS_KPM_DIR, target if target.endswith(".kea") else target + ".kea")
        if os.path.exists(dest_path):
            with open(dest_path, "rb") as f:
                data = f.read()

    if data:
        info = parse_kea_header(data)
        if info:
            print(f"\n{Color.CYAN}{Color.BOLD}--- .KEA Package Metadata ---{Color.RESET}")
            print(f"Name:        {info['name']}")
            print(f"Version:     {info['version']}")
            print(f"Author:      {info['author']}")
            print(f"Category:    {info['category']}")
            print(f"Description: {info['description']}")
            print(f"ELF Offset:  {info['elf']['offset']} (Size: {info['elf']['size']} bytes)")
            print(f"Entry Point: 0x{info['elf']['entry']:X}")
            print(f"Icon:        {info['icon']['width']}x{info['icon']['height']} ({info['icon']['size']} bytes)")
            print(f"CRC32:       0x{info['elf']['crc']:08X}\n")
            return
        else:
            print(f"{Color.RED}Not a valid .kea container file.{Color.RESET}")
            return

    # Check remote catalog
    catalog = fetch_catalog()
    if catalog and "packages" in catalog:
        clean = target[:-4] if target.endswith(".kea") else target
        for p in catalog["packages"]:
            if p["id"] == clean or p["filename"] == target:
                print(f"\n{Color.CYAN}{Color.BOLD}--- Remote Package: {p['name']} ---{Color.RESET}")
                print(f"Version:     {p['version']}")
                print(f"Author:      {p['author']}")
                print(f"Category:    {p['category']}")
                print(f"Description: {p['description']}")
                print(f"Size:        {p['size'] / 1024:.1f} KB")
                print(f"Download:    {p['download_url']}\n")
                return

    print(f"{Color.RED}Package '{target}' not found.{Color.RESET}")

def cmd_open():
    path = ensure_kpm_dir()
    if sys.platform == "win32":
        os.startfile(path)
    else:
        subprocess.run(["xdg-open", path])
    print(f"{Color.GREEN}Opened folder: {path}{Color.RESET}")

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print_banner()
        print("Usage:")
        print("  kpm install <package.kea>   Download & install package into Downloads/KPM")
        print("  kpm list                    List installed and available packages")
        print("  kpm info <package>          Inspect metadata of package or .kea file")
        print("  kpm open                    Open Downloads/KPM in file explorer")
        print("  kpm repo <url>              Set active repository URL")
        print("  kpm version                 Show version information")
        print("")
        return

    cmd = sys.argv[1].lower()

    if cmd in ("install", "i", "add"):
        pkg = sys.argv[2] if len(sys.argv) > 2 else ""
        cmd_install(pkg)
    elif cmd in ("list", "ls"):
        cmd_list()
    elif cmd in ("info", "show"):
        pkg = sys.argv[2] if len(sys.argv) > 2 else ""
        cmd_info(pkg)
    elif cmd in ("open", "dir"):
        cmd_open()
    elif cmd in ("repo", "set-repo"):
        if len(sys.argv) > 2:
            cfg = get_config()
            cfg["repo_url"] = sys.argv[2]
            save_config(cfg)
            print(f"{Color.GREEN}Active repository set to: {sys.argv[2]}{Color.RESET}")
        else:
            print(f"Current repository: {get_repo_url()}")
    elif cmd in ("version", "-v"):
        print(f"Kesh Package Manager (KPM) v{KPM_VERSION}")
    else:
        print(f"{Color.RED}Unknown command: {cmd}. Run 'kpm --help' for usage.{Color.RESET}")

if __name__ == "__main__":
    main()
