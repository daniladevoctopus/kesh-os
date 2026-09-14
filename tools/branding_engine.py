#!/usr/bin/env python3
import argparse
import os
import sys
from pathlib import Path

BRAND_NAME = "KeshOS"
BRAND_VERSION = "Beta 0.8.0"
BRAND_CODENAME = "Brownie"

EXCLUDED_DIR_NAMES = {
    ".git",
    ".svn",
    ".hg",
    "CMakeFiles",
    "output",
    "output-*",
    "build",
    "_build",
    "bin",
    "obj",
    "ntoskrnl",
    "boot",
    "drivers",
    "sdk/tools",
    "sdk\tools",
    "host-tools",
    "toolchain",
    "third_party",
    "vendor",
}

SAFE_FILE_SUFFIXES = {".rc", ".h", ".inf"}
SAFE_PATH_HINTS = (
    "/lang/",
    "base/",
    "dll/",
    "media/",
    "sdk/include/reactos/",
    "win32ss/",
    "subsystems/",
)

BRAND_REPLACEMENTS = [
    ("ReactOS Project", "KeshOS Project"),
    ("ReactOS Operating System", "KeshOS Operating System"),
    ("ReactOS Core Component", "KeshOS Core Component"),
    ("ReactOS Applications Manager", "KeshOS App Manager"),
    ("ReactOS Core", "KeshOS Core"),
    ("\"ReactOS\"", "\"KeshOS\""),
    ("ReactOS Setup", "KeshOS Setup"),
    ("This is ReactOS", "This is KeshOS"),
    ("ReactOS is a registered trademark", "KeshOS is an independent operating system by DanDevXP"),
    ("ReactOS is an operating system", "KeshOS is an operating system"),
    ("ReactOS", "KeshOS"),
]


def log(msg):
    print(msg)


def safe_rel_path(path: Path, root_dir: Path) -> str:
    try:
        return str(path.relative_to(root_dir)).replace("\\", "/")
    except ValueError:
        return str(path).replace("\\", "/")


def should_skip(path: Path, root_dir: Path) -> bool:
    rel = safe_rel_path(path, root_dir)
    parts = [p.lower() for p in Path(rel).parts]
    if any(part in {".git", "cmakefiles", "ntoskrnl", "boot", "output", "build", "_build", "obj", "bin"} for part in parts):
        return True
    if any(part.startswith("output-") for part in parts):
        return True
    if not path.suffix.lower() in SAFE_FILE_SUFFIXES:
        return True
    if "/lang/" in rel.lower() or "\\lang\\" in rel.lower():
        return False
    if not any(hint in rel.lower() for hint in SAFE_PATH_HINTS):
        return True
    return False


def collect_branding_candidates(root_dir: Path):
    candidates = []
    for path in root_dir.rglob("*"):
        if not path.is_file():
            continue
        if should_skip(path, root_dir):
            continue
        candidates.append(path)
    return sorted(candidates)


def read_text_preserve_encoding(path: Path):
    encodings = ["utf-8", "utf-8-sig", "cp1251", "latin-1"]
    last_error = None
    for enc in encodings:
        try:
            with path.open("r", encoding=enc, errors="strict") as f:
                return f.read(), enc
        except Exception as exc:  # pragma: no cover
            last_error = exc
    raise UnicodeDecodeError("utf-8", b"", 0, 1, str(last_error) or "unknown decode issue")


def apply_replacements(content: str):
    patched = content
    for old, new in BRAND_REPLACEMENTS:
        patched = patched.replace(old, new)
    return patched


def patch_file(path: Path, root_dir: Path, dry_run: bool = False):
    rel = safe_rel_path(path, root_dir)
    try:
        content, encoding = read_text_preserve_encoding(path)
    except Exception as exc:
        log(f"[-] [decode] {rel} :: {exc}")
        return False

    patched = apply_replacements(content)
    if patched == content:
        log(f"[=] [unchanged] {rel}")
        return False

    if dry_run:
        log(f"[--] [dry-run] {rel} :: would be branded to {BRAND_NAME}")
        return True

    try:
        with path.open("w", encoding=encoding, errors="strict") as f:
            f.write(patched)
        log(f"[+] [patched] {rel} :: {BRAND_NAME} branding applied")
        return True
    except Exception as exc:
        log(f"[!] [write-error] {rel} :: {exc}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Safe KeshOS branding engine for ReactOS-derived source trees")
    parser.add_argument("--root", default=".", help="Project root to scan")
    parser.add_argument("--dry-run", action="store_true", help="Show what would change without modifying files")
    args = parser.parse_args()

    root_dir = Path(args.root).resolve()
    log("==============================================================")
    log(f"   KeshOS Branding Engine v1.1 [codename: {BRAND_CODENAME}]")
    log(f"   Safe UI-text scan for: {root_dir}")
    log("==============================================================")
    log(f"[#] Brand: {BRAND_NAME} {BRAND_VERSION} ({BRAND_CODENAME})")

    candidates = collect_branding_candidates(root_dir)
    if not candidates:
        log("[-] No safe branding candidates found under the allowed UI paths.")
        return 1

    patched_count = 0
    for path in candidates:
        if patch_file(path, root_dir, dry_run=args.dry_run):
            patched_count += 1

    log("==============================================================")
    if args.dry_run:
        log(f"   Dry run complete. Safe candidates identified: {len(candidates)}")
        log(f"   Files that would be changed: {patched_count}")
    else:
        log(f"   Branding pass complete. Files changed: {patched_count}")
    log("   The engine intentionally skips core build paths and system internals.")
    log("==============================================================")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        log("\n[!] Branding cancelled by user.")
        raise SystemExit(130)
