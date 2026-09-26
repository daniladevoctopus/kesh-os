#!/usr/bin/env python3
"""
tools/kea-pack.py - Kesh Executable Application (.KEA) Packager

Bundles 64-bit ELF executable, high-resolution RGBA icon, and KeshOS metadata
into a unified standalone .kea binary container.
"""

import sys
import os
import struct
import zlib
import argparse

KEA_MAGIC = 0x0141454B  # "KEA\1"
KEA_VERSION = 1

# Permission bits
PERM_NETWORK = (1 << 0)
PERM_FS      = (1 << 1)
PERM_SOUND   = (1 << 2)
PERM_GUI     = (1 << 3)
PERM_ROOT    = (1 << 4)

HEADER_FORMAT = "<IIHH32s16s32s16s64sIIIHHQQQI8I"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

def parse_bmp_to_rgba(bmp_path):
    if not os.path.exists(bmp_path):
        return 32, 32, b'\x00' * (32 * 32 * 4)
    with open(bmp_path, "rb") as f:
        data = f.read()
    if len(data) < 54 or data[:2] != b'BM':
        return 32, 32, b'\x00' * (32 * 32 * 4)
    
    offset = struct.unpack_from("<I", data, 10)[0]
    w = struct.unpack_from("<i", data, 18)[0]
    h = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    
    # We output raw 32-bit RGBA
    rgba = bytearray(w * h * 4)
    row_bytes = ((w * bpp + 31) // 32) * 4
    
    for y in range(h):
        src_y = h - 1 - y  # BMP bottom-up
        row_start = offset + src_y * row_bytes
        for x in range(w):
            dst_idx = (y * w + x) * 4
            if bpp == 32 and row_start + x * 4 + 3 < len(data):
                b = data[row_start + x * 4]
                g = data[row_start + x * 4 + 1]
                r = data[row_start + x * 4 + 2]
                a = data[row_start + x * 4 + 3]
                rgba[dst_idx:dst_idx+4] = bytes([r, g, b, a])
            elif bpp == 24 and row_start + x * 3 + 2 < len(data):
                b = data[row_start + x * 3]
                g = data[row_start + x * 3 + 1]
                r = data[row_start + x * 3 + 2]
                rgba[dst_idx:dst_idx+4] = bytes([r, g, b, 255])
    return w, h, bytes(rgba)

def pack_kea(elf_path, out_path, name="App", version="1.0.0", author="KeshOS Developer",
             category="Utilities", desc="", icon_path=None, perms=(PERM_GUI | PERM_FS)):
    if not os.path.exists(elf_path):
        raise FileNotFoundError(f"ELF binary not found: {elf_path}")
    
    with open(elf_path, "rb") as f:
        elf_data = f.read()
    
    if len(elf_data) < 64 or elf_data[:4] != b'\x7fELF':
        raise ValueError(f"File {elf_path} is not a valid ELF executable")
    
    # Extract ELF64 entry point
    entry_point = struct.unpack_from("<Q", elf_data, 24)[0]
    elf_size = len(elf_data)
    elf_crc = zlib.crc32(elf_data) & 0xFFFFFFFF

    # Process Icon
    icon_w, icon_h, icon_data = 32, 32, b'\x00' * (32 * 32 * 4)
    if icon_path and os.path.exists(icon_path):
        icon_w, icon_h, icon_data = parse_bmp_to_rgba(icon_path)
    
    icon_size = len(icon_data)
    icon_offset = HEADER_SIZE
    elf_offset = icon_offset + icon_size

    # Prepare strings
    b_name = name.encode('utf-8')[:31]
    b_ver = version.encode('utf-8')[:15]
    b_author = author.encode('utf-8')[:31]
    b_cat = category.encode('utf-8')[:15]
    b_desc = desc.encode('utf-8')[:63]

    reserved = [0] * 8

    header = struct.pack(
        HEADER_FORMAT,
        KEA_MAGIC,
        HEADER_SIZE,
        KEA_VERSION,
        0,              # flags
        b_name,
        b_ver,
        b_author,
        b_cat,
        b_desc,
        perms,
        icon_offset,
        icon_size,
        icon_w,
        icon_h,
        elf_offset,
        elf_size,
        entry_point,
        elf_crc,
        *reserved
    )

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(header)
        f.write(icon_data)
        f.write(elf_data)
    
    total_size = len(header) + len(icon_data) + len(elf_data)
    print(f"[KEA-PACK] Successfully generated {out_path} ({total_size} bytes)")
    print(f"           App: {name} v{version} by {author}")
    print(f"           Entry Point: 0x{entry_point:X}, ELF size: {elf_size} bytes, CRC32: 0x{elf_crc:08X}")

def main():
    parser = argparse.ArgumentParser(description="Kesh Executable Application (.kea) Packager")
    parser.add_argument("--elf", required=True, help="Input ELF64 binary")
    parser.add_argument("--out", required=True, help="Output .kea file")
    parser.add_argument("--name", default="App", help="Application name")
    parser.add_argument("--ver", default="1.0.0", help="Application version")
    parser.add_argument("--author", default="KeshOS Developer", help="Author name")
    parser.add_argument("--category", default="Utilities", help="Category")
    parser.add_argument("--desc", default="", help="Short description")
    parser.add_argument("--icon", default=None, help="Optional icon BMP file")

    args = parser.parse_args()
    pack_kea(
        elf_path=args.elf,
        out_path=args.out,
        name=args.name,
        version=args.ver,
        author=args.author,
        category=args.category,
        desc=args.desc,
        icon_path=args.icon
    )

if __name__ == "__main__":
    main()
