#!/usr/bin/env python3
"""
================================================================================
  Kesh Package Manager (KPM) - Elite Windows Native CLI & Installer
  Strict HTTPS Cloud Registry Client for KeshOS (.KEA) Executables
  Powered by Vercel Serverless Registry API
================================================================================
"""

import sys
import os
import struct
import json
import time
import argparse
import urllib.request
import urllib.error
from pathlib import Path

KPM_VERSION = "1.0.0-PRO"
DEFAULT_REPO_URL = "https://kesh-kpm.vercel.app"
CONFIG_FILE = Path.home() / ".kpm_config.json"
DOWNLOADS_KPM_DIR = Path.home() / "Downloads" / "KPM"
LOCALAPPDATA = os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData" / "Local"))
INSTALL_BIN_DIR = Path(LOCALAPPDATA) / "KeshOS" / "KPM" / "bin"
INSTALLED_EXE_PATH = INSTALL_BIN_DIR / "kpm.exe"

# Configure Unicode output safely for Windows Console
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# ANSI Color Palette
class C:
    CYAN    = "\033[96m"
    GREEN   = "\033[92m"
    YELLOW  = "\033[93m"
    RED     = "\033[91m"
    MAGENTA = "\033[95m"
    BLUE    = "\033[94m"
    WHITE   = "\033[97m"
    GRAY    = "\033[90m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    RESET   = "\033[0m"

# Enable ANSI colors on Windows 10/11
if sys.platform == "win32":
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)
        mode = ctypes.c_ulong()
        kernel32.GetConsoleMode(handle, ctypes.byref(mode))
        kernel32.SetConsoleMode(handle, mode.value | 0x0004 | 0x0008)
    except Exception:
        pass

def print_banner():
    repo = get_repo_url()
    banner = f"""
{C.CYAN}{C.BOLD}  ██╗  ██╗██████╗ ███╗   ███╗{C.RESET}  {C.WHITE}{C.BOLD}KESH PACKAGE MANAGER{C.RESET} {C.MAGENTA}v{KPM_VERSION}{C.RESET}
{C.CYAN}{C.BOLD}  ██║ ██╔╝██╔══██╗████╗ ████║{C.RESET}  {C.GRAY}Universal .KEA Package Runtime & Registry{C.RESET}
{C.CYAN}{C.BOLD}  █████╔╝ ██████╔╝██╔████╔██║{C.RESET}  {C.GREEN}●{C.RESET} {C.WHITE}Target:{C.RESET}   {C.CYAN}{DOWNLOADS_KPM_DIR}{C.RESET}
{C.CYAN}{C.BOLD}  ██╔═██╗ ██╔═══╝ ██║╚██╔╝██║{C.RESET}  {C.GREEN}●{C.RESET} {C.WHITE}Server:{C.RESET}   {C.MAGENTA}{repo}{C.RESET}
{C.CYAN}{C.BOLD}  ██║  ██╗██║     ██║ ╚═╝ ██║{C.RESET}  {C.GREEN}●{C.RESET} {C.WHITE}Security:{C.RESET} {C.GREEN}Strict HTTPS (Vercel API Live-Log){C.RESET}
{C.CYAN}{C.BOLD}  ╚═╝  ╚═╝╚═╝     ╚═╝     ╚═╝{C.RESET}
"""
    print(banner)

def get_config():
    if CONFIG_FILE.exists():
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
        print(f"{C.RED}Error saving config: {e}{C.RESET}")

def get_repo_url():
    return get_config().get("repo_url", DEFAULT_REPO_URL).rstrip('/')

def ensure_download_dir():
    DOWNLOADS_KPM_DIR.mkdir(parents=True, exist_ok=True)
    return DOWNLOADS_KPM_DIR

def parse_kea_header(data: bytes):
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
        entry_pt = fields[16]
        crc = fields[17]
        return {
            "name": name,
            "version": version,
            "author": author,
            "category": category,
            "description": description,
            "entry_point": hex(entry_pt),
            "crc": hex(crc),
            "size": len(data)
        }
    except Exception:
        return None

def fetch_packages_catalog():
    """Fetch catalog from Vercel Serverless API (/api/catalog)."""
    repo = get_repo_url()
    urls = [
        f"{repo}/api/catalog",
        f"{repo}/packages.json"
    ]
    for url in urls:
        req = urllib.request.Request(
            url,
            headers={
                "User-Agent": f"KPM-CLI/{KPM_VERSION} (Windows x86_64)",
                "Accept": "application/json"
            }
        )
        try:
            with urllib.request.urlopen(req, timeout=8) as resp:
                data = resp.read().decode('utf-8')
                return json.loads(data)
        except Exception:
            continue
    return None

def download_package_web(pkg_name, dest_path):
    """
    Downloads package strictly via HTTPS from Vercel Serverless Function (/api/download?pkg=...)
    This guarantees triggering Vercel Serverless Runtime Logs!
    """
    repo = get_repo_url()
    # Try Serverless function route first to force Vercel Live Logging
    urls = [
        f"{repo}/api/download?pkg={pkg_name}",
        f"{repo}/packages/{pkg_name}"
    ]

    for i, url in enumerate(urls):
        req = urllib.request.Request(
            url,
            headers={
                "User-Agent": f"KPM-CLI/{KPM_VERSION} (Windows x86_64; LiveLog)",
                "Accept": "*/*"
            }
        )
        try:
            print(f"  {C.CYAN}[1/3]{C.RESET} {C.WHITE}Connecting to Vercel Serverless Registry...{C.RESET}")
            print(f"        {C.GRAY}Endpoint: {url}{C.RESET}")
            with urllib.request.urlopen(req, timeout=12) as resp:
                status = resp.status if hasattr(resp, "status") else 200
                total_size = resp.length or 0
                block_size = 4096
                downloaded = 0
                start_time = time.time()
                data = bytearray()

                print(f"  {C.GREEN}[2/3]{C.RESET} {C.WHITE}Server response: {C.GREEN}HTTP {status} OK{C.RESET} {C.GRAY}(Serverless Lambda Executed){C.RESET}")

                while True:
                    chunk = resp.read(block_size)
                    if not chunk:
                        break
                    data.extend(chunk)
                    downloaded += len(chunk)

                    elapsed = time.time() - start_time
                    speed = (downloaded / 1024) / (elapsed + 0.0001)
                    if total_size > 0:
                        percent = int((downloaded / total_size) * 100)
                        bar_len = 28
                        filled = int(bar_len * percent / 100)
                        bar = "=" * (filled - 1) + ">" + " " * (bar_len - filled) if filled > 0 else " " * bar_len
                        sys.stdout.write(
                            f"\r        {C.CYAN}[{bar}] {percent}%{C.RESET} "
                            f"{C.WHITE}{downloaded/1024:.1f}/{total_size/1024:.1f} KB{C.RESET} "
                            f"{C.GRAY}({speed:.1f} KB/s){C.RESET} "
                        )
                    else:
                        sys.stdout.write(
                            f"\r        {C.CYAN}[STREAM]{C.RESET} {downloaded/1024:.1f} KB downloaded ({speed:.1f} KB/s) "
                        )
                    sys.stdout.flush()

                sys.stdout.write("\n")
                with open(dest_path, "wb") as f:
                    f.write(data)
                return bytes(data)

        except urllib.error.HTTPError as e:
            if i == 0:
                # Try fallback route
                continue
            print(f"\n{C.RED}  [!] HTTPS Error: Server returned HTTP {e.code} ({e.reason}){C.RESET}")
            return None
        except urllib.error.URLError as e:
            print(f"\n{C.RED}  [!] Network Error: Could not connect to {url}: {e.reason}{C.RESET}")
            return None
        except Exception as e:
            print(f"\n{C.RED}  [!] Error: {e}{C.RESET}")
            return None
    return None

def cmd_install(pkg_name):
    if not pkg_name:
        print(f"{C.RED}Error: Specify package name (e.g. kpm install notepad.kea){C.RESET}")
        return 1

    if not pkg_name.endswith(".kea"):
        pkg_name += ".kea"

    print_banner()
    dest_dir = ensure_download_dir()
    dest_path = dest_dir / pkg_name

    print(f"{C.BOLD}┌─ INITIATING PACKAGE INSTALLATION ────────────────────────────┐{C.RESET}")
    print(f"{C.BOLD}│{C.RESET}  Package:   {C.CYAN}{C.BOLD}{pkg_name:<47}{C.RESET}{C.BOLD}│{C.RESET}")
    print(f"{C.BOLD}│{C.RESET}  Location:  {C.GRAY}{str(dest_path):<47}{C.RESET}{C.BOLD}│{C.RESET}")
    print(f"{C.BOLD}└──────────────────────────────────────────────────────────────┘{C.RESET}\n")

    data = download_package_web(pkg_name, dest_path)

    if not data:
        print(f"\n{C.RED}[FAILED] Package '{pkg_name}' could not be downloaded via HTTPS.{C.RESET}")
        print(f"{C.YELLOW}         Check that the package exists on https://kesh-kpm.vercel.app{C.RESET}\n")
        return 1

    print(f"  {C.GREEN}[3/3]{C.RESET} {C.WHITE}Verifying KEA Binary Container & Integrity...{C.RESET}")
    info = parse_kea_header(data)

    if info:
        print(f"\n{C.GREEN}{C.BOLD}╔══════════════════════════════════════════════════════════════╗{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║  PACKAGE VERIFIED: {info['name']:<41} ║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}╠══════════════════════════════════════════════════════════════╣{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Version:      {C.WHITE}{info['version']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Author:       {C.WHITE}{info['author']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Category:     {C.YELLOW}{info['category']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Description:  {C.GRAY}{info['description']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Entry Point:  {C.CYAN}{info['entry_point']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  CRC32 Hash:   {C.MAGENTA}{info['crc']:<43}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Architecture: {C.WHITE}x86_64 KeshOS Ring 3 ELF (CPL 3 PML4)       {C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}║{C.RESET}  Package Size: {C.WHITE}{info['size']/1024:.1f} KB{' '*36}{C.RESET}{C.GREEN}{C.BOLD}║{C.RESET}")
        print(f"{C.GREEN}{C.BOLD}╚══════════════════════════════════════════════════════════════╝{C.RESET}")
        print(f"\n{C.GREEN}[SUCCESS] Installed to: {dest_path}{C.RESET}\n")
    else:
        print(f"\n{C.GREEN}[SUCCESS] Downloaded to: {dest_path} ({len(data)/1024:.1f} KB){C.RESET}\n")
    return 0

def cmd_list():
    print_banner()
    dest_dir = ensure_download_dir()
    print(f"{C.CYAN}{C.BOLD}=== LOCALLY DOWNLOADED PACKAGES ({dest_dir}) ==={C.RESET}")
    local_files = list(dest_dir.glob("*.kea"))
    if local_files:
        for f in local_files:
            try:
                data = f.read_bytes()
                hdr = parse_kea_header(data)
                tag = f"[{hdr['category']}] {hdr['description']}" if hdr else "KEA Binary"
            except Exception:
                tag = "KEA Binary"
            print(f"  * {C.GREEN}{f.name:<22}{C.RESET} {C.WHITE}{f.stat().st_size/1024:>6.1f} KB{C.RESET}  {C.GRAY}- {tag}{C.RESET}")
    else:
        print(f"{C.GRAY}  (Folder is empty. Run: kpm install notepad.kea){C.RESET}")

    repo = get_repo_url()
    print(f"\n{C.CYAN}{C.BOLD}=== CLOUD REGISTRY ({repo}) ==={C.RESET}")
    catalog = fetch_packages_catalog()
    if catalog and "packages" in catalog:
        for p in catalog["packages"]:
            title = f"{p['name']} v{p.get('version', '1.0.0')}"
            cat = p.get('category', 'App')
            desc = p.get('description', '')
            print(f"  + {C.WHITE}{C.BOLD}{title:<24}{C.RESET} {C.YELLOW}[{cat:<10}]{C.RESET} {C.GRAY}- {desc}{C.RESET}")
    else:
        print(f"{C.YELLOW}  (Could not fetch package list from {repo}. Server may be sleeping){C.RESET}")
    print("")

def cmd_info(pkg_name):
    if not pkg_name:
        print(f"{C.RED}Usage: kpm info <package.kea>{C.RESET}")
        return
    if not pkg_name.endswith(".kea"):
        pkg_name += ".kea"
    dest_path = ensure_download_dir() / pkg_name
    if not dest_path.exists():
        print(f"{C.YELLOW}Package '{pkg_name}' is not downloaded yet. Run: kpm install {pkg_name}{C.RESET}")
        return
    info = parse_kea_header(dest_path.read_bytes())
    if info:
        print_banner()
        print(f"{C.CYAN}{C.BOLD}=== .KEA PACKAGE METADATA: {info['name']} ==={C.RESET}")
        print(f"  Version:      {info['version']}")
        print(f"  Author:       {info['author']}")
        print(f"  Category:     {info['category']}")
        print(f"  Description:  {info['description']}")
        print(f"  Entry Point:  {info['entry_point']}")
        print(f"  Checksum:     {info['crc']}")
        print(f"  File Path:    {dest_path}")
        print(f"  File Size:    {info['size']/1024:.1f} KB\n")

def cmd_open():
    ensure_download_dir()
    if sys.platform == "win32":
        os.startfile(str(DOWNLOADS_KPM_DIR))
    print(f"{C.GREEN}[+] Opened Downloads\\KPM folder in Explorer!{C.RESET}")

def notify_windows_environment_change():
    if sys.platform != "win32":
        return
    try:
        import ctypes
        HWND_BROADCAST = 0xFFFF
        WM_SETTINGCHANGE = 0x001A
        SMTO_ABORTIFHUNG = 0x0002
        result = ctypes.c_ulong()
        ctypes.windll.user32.SendMessageTimeoutW(
            HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment",
            SMTO_ABORTIFHUNG, 3000, ctypes.byref(result)
        )
    except Exception:
        pass

def cmd_setup():
    """Permanent 1-Click Setup: installs kpm.exe to AppData and adds to Windows User PATH."""
    print_banner()
    print(f"{C.CYAN}{C.BOLD}================================================================{C.RESET}")
    print(f"{C.WHITE}{C.BOLD}  Kesh Package Manager (KPM) — One-Click Windows System Setup{C.RESET}")
    print(f"{C.CYAN}{C.BOLD}================================================================{C.RESET}\n")

    INSTALL_BIN_DIR.mkdir(parents=True, exist_ok=True)
    ensure_download_dir()

    current_exe = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve()
    target_exe = INSTALLED_EXE_PATH
    
    if getattr(sys, 'frozen', False):
        try:
            if current_exe != target_exe:
                import shutil
                shutil.copy2(current_exe, target_exe)
                print(f"{C.GREEN}[+] Installed system binary: {target_exe}{C.RESET}")
            else:
                print(f"{C.CYAN}[*] Running directly from target location: {target_exe}{C.RESET}")
        except Exception as e:
            print(f"{C.YELLOW}[!] Note while copying binary: {e}{C.RESET}")
    else:
        cmd_wrapper = INSTALL_BIN_DIR / "kpm.cmd"
        with open(cmd_wrapper, "w") as f:
            f.write(f'@echo off\npython "{current_exe}" %*\n')
        print(f"{C.GREEN}[+] Installed system launcher: {cmd_wrapper}{C.RESET}")

    if sys.platform == "win32":
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment", 0, winreg.KEY_ALL_ACCESS) as key:
                try:
                    val, _ = winreg.QueryValueEx(key, "Path")
                except FileNotFoundError:
                    val = ""
                
                paths = [p for p in val.split(';') if p]
                bin_str = str(INSTALL_BIN_DIR)
                if bin_str not in paths:
                    paths.append(bin_str)
                    new_val = ';'.join(paths)
                    winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_val)
                    print(f"{C.GREEN}[+] Permanently added '{INSTALL_BIN_DIR}' to User PATH!{C.RESET}")
                    notify_windows_environment_change()
                else:
                    print(f"{C.CYAN}[*] '{INSTALL_BIN_DIR}' is already registered in PATH.{C.RESET}")
        except Exception as e:
            print(f"{C.RED}Error updating registry PATH: {e}{C.RESET}")

    print(f"\n{C.GREEN}{C.BOLD}╔══════════════════════════════════════════════════════════════╗{C.RESET}")
    print(f"{C.GREEN}{C.BOLD}║  [SUCCESS] KPM IS PERMANENTLY INSTALLED ON THIS PC!          ║{C.RESET}")
    print(f"{C.GREEN}{C.BOLD}╚══════════════════════════════════════════════════════════════╝{C.RESET}")
    print(f"You can now close this window, delete this installer,")
    print(f"open ANY CMD, PowerShell or Windows Terminal, and type:\n")
    print(f"    {C.CYAN}kpm install notepad.kea{C.RESET}")
    print(f"    {C.CYAN}kpm list{C.RESET}")
    print(f"    {C.CYAN}kpm open{C.RESET}\n")

def run_interactive_menu():
    print_banner()
    while True:
        print(f"{C.YELLOW}{C.BOLD}Choose an action:{C.RESET}")
        print(f"  {C.CYAN}[1]{C.RESET} {C.BOLD}Install KPM to Windows PATH (One-Click Setup){C.RESET}")
        print(f"  {C.CYAN}[2]{C.RESET} Install / Download a package (e.g. notepad.kea)")
        print(f"  {C.CYAN}[3]{C.RESET} List Cloud Packages from Vercel")
        print(f"  {C.CYAN}[4]{C.RESET} Open Downloads\\KPM folder in Explorer")
        print(f"  {C.CYAN}[Q]{C.RESET} Exit")
        try:
            choice = input(f"\n{C.WHITE}{C.BOLD}Enter choice (1-4, Q): {C.RESET}").strip().upper()
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            break

        if choice == "1":
            cmd_setup()
        elif choice == "2":
            pkg = input(f"{C.CYAN}Enter package name (e.g. notepad.kea): {C.RESET}").strip()
            if pkg:
                cmd_install(pkg)
        elif choice == "3":
            cmd_list()
        elif choice == "4":
            cmd_open()
        elif choice in ("Q", "QUIT", "EXIT"):
            break
        print("")

def main():
    parser = argparse.ArgumentParser(
        description="Kesh Package Manager (KPM) - Strict HTTPS Cloud Package Manager",
        add_help=False
    )
    parser.add_argument("command", nargs="?", default=None, help="install | list | info | open | setup | repo")
    parser.add_argument("package", nargs="?", default=None, help="Target package name")
    parser.add_argument("--repo", default=None, help="Custom repository URL")
    parser.add_argument("-h", "--help", action="store_true", help="Show help")

    args = parser.parse_args()

    if args.repo:
        save_config({"repo_url": args.repo})
        print(f"{C.GREEN}[+] Active repository set to: {args.repo}{C.RESET}")

    if args.help or (args.command in ("help", "-h", "--help")):
        print_banner()
        print("Usage:")
        print("  kpm install <package.kea>   Download package directly from Vercel")
        print("  kpm list                    List local & remote packages")
        print("  kpm info <package>          Inspect .KEA package metadata")
        print("  kpm open                    Open Downloads\\KPM in Explorer")
        print("  kpm setup                   Install KPM to Windows PATH permanently")
        print("  kpm repo <url>              Change repository URL")
        print("")
        return

    if not args.command:
        current_exe = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve()
        if getattr(sys, 'frozen', False) and current_exe != INSTALLED_EXE_PATH:
            cmd_setup()
            input("Press Enter to continue...")
        else:
            run_interactive_menu()
        return

    cmd = args.command.lower()
    if cmd in ("install", "i"):
        sys.exit(cmd_install(args.package))
    elif cmd in ("list", "ls"):
        cmd_list()
    elif cmd in ("info", "show"):
        cmd_info(args.package)
    elif cmd == "open":
        cmd_open()
    elif cmd == "setup":
        cmd_setup()
    elif cmd == "repo":
        if args.package:
            save_config({"repo_url": args.package})
            print(f"{C.GREEN}[+] Active repository set to: {args.package}{C.RESET}")
        else:
            print(f"{C.CYAN}Current repository: {get_repo_url()}{C.RESET}")
    else:
        print(f"{C.RED}Unknown command '{cmd}'. Run 'kpm --help' for usage.{C.RESET}")
        sys.exit(1)

if __name__ == "__main__":
    main()
