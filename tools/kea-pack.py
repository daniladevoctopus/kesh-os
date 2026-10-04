#!/usr/bin/env python3
import argparse
import hashlib
import os
import struct
import zlib
from kpm_signing import load_key, sign

MAGIC = 0x0141454B
BASE_FORMAT = "<IIHH32s16s32s16s64sIIIHHQQQI8I"
V2_FORMAT = BASE_FORMAT + "IHHIHH16s32s"
BASE_SIZE = struct.calcsize(BASE_FORMAT)
V2_SIZE = struct.calcsize(V2_FORMAT)
DEPENDENCY_FORMAT = "<32s16s"

def fixed(value, size):
    return value.encode("utf-8")[:size - 1]

def bmp_rgba(path):
    if not path or not os.path.exists(path):
        return 32, 32, b"\0" * 4096
    data = open(path, "rb").read()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("invalid BMP icon")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    signed_height = struct.unpack_from("<i", data, 22)[0]
    height = abs(signed_height)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if width <= 0 or height <= 0 or bpp not in (24, 32):
        raise ValueError("unsupported BMP icon")
    stride = ((width * bpp + 31) // 32) * 4
    rgba = bytearray(width * height * 4)
    for y in range(height):
        source_y = height - 1 - y if signed_height > 0 else y
        for x in range(width):
            source = offset + source_y * stride + x * (bpp // 8)
            target = (y * width + x) * 4
            b, g, r = data[source:source + 3]
            a = data[source + 3] if bpp == 32 else 255
            rgba[target:target + 4] = bytes((r, g, b, a))
    return width, height, bytes(rgba)

def dependency_blob(values):
    result = bytearray()
    for value in values:
        name, separator, version = value.partition("@")
        if not separator or not name or not version or len(name) > 31 or len(version) > 15:
            raise ValueError("dependency must be name@minimum-version")
        result += struct.pack(DEPENDENCY_FORMAT, fixed(name, 32), fixed(version, 16))
    return bytes(result)

def base_fields(args, header_size, version, flags, icon_offset, icon, icon_w, icon_h, elf_offset, elf):
    return (MAGIC, header_size, version, flags, fixed(args.name, 32), fixed(args.ver, 16),
            fixed(args.author, 32), fixed(args.category, 16), fixed(args.desc, 64), args.perms,
            icon_offset, len(icon), icon_w, icon_h, elf_offset, len(elf),
            struct.unpack_from("<Q", elf, 24)[0], zlib.crc32(elf) & 0xffffffff, *([0] * 8))

def pack(args):
    elf = open(args.elf, "rb").read()
    if len(elf) < 64 or elf[:4] != b"\x7fELF":
        raise ValueError("input is not an ELF64 executable")
    icon_w, icon_h, icon = bmp_rgba(args.icon)
    dependencies = dependency_blob(args.depends)
    if not args.signing_key:
        icon_offset = BASE_SIZE
        elf_offset = icon_offset + len(icon)
        header = struct.pack(BASE_FORMAT, *base_fields(args, BASE_SIZE, 1, 0, icon_offset, icon, icon_w, icon_h, elf_offset, elf))
        package = header + icon + elf
    else:
        key = load_key(args.signing_key)
        public_key = bytes.fromhex(key["public"])
        key_id = hashlib.sha256(public_key).digest()[:16]
        icon_offset = V2_SIZE
        dependency_offset = icon_offset + len(icon)
        elf_offset = dependency_offset + len(dependencies)
        signature_offset = elf_offset + len(elf)
        fields = base_fields(args, V2_SIZE, 2, 1, icon_offset, icon, icon_w, icon_h, elf_offset, elf)
        header = struct.pack(V2_FORMAT, *fields, dependency_offset, len(args.depends), struct.calcsize(DEPENDENCY_FORMAT),
                             signature_offset, 64, 1, key_id, b"\0" * 32)
        unsigned = header + icon + dependencies + elf
        digest = hashlib.sha256(unsigned).digest()
        header = struct.pack(V2_FORMAT, *fields, dependency_offset, len(args.depends), struct.calcsize(DEPENDENCY_FORMAT),
                             signature_offset, 64, 1, key_id, digest)
        package = header + icon + dependencies + elf + sign(int(key["private"], 16), digest)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as stream:
        stream.write(package)
    print(f"KEA {args.name} {args.ver}: {len(package)} bytes -> {args.out}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--name", default="App")
    parser.add_argument("--ver", default="1.0.0")
    parser.add_argument("--author", default="KeshOS Developer")
    parser.add_argument("--category", default="Utilities")
    parser.add_argument("--desc", default="")
    parser.add_argument("--icon")
    parser.add_argument("--perms", type=lambda value: int(value, 0), default=10)
    parser.add_argument("--depends", action="append", default=[])
    parser.add_argument("--signing-key", default=os.environ.get("KPM_SIGNING_KEY"))
    pack(parser.parse_args())

if __name__ == "__main__":
    main()
