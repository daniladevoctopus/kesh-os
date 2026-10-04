#!/usr/bin/env python3
import argparse
import hashlib
import os
import struct
from kpm_signing import load_key, sign

FORMAT = "<IHH32sQQIHH16s32s16s"
SIZE = struct.calcsize(FORMAT)

def fixed(value, size):
    return value.encode("utf-8")[:size - 1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--signing-key", default=os.environ.get("KPM_SIGNING_KEY"), required=False)
    args = parser.parse_args()
    if not args.signing_key:
        parser.error("--signing-key or KPM_SIGNING_KEY is required")
    payload = open(args.image, "rb").read()
    key = load_key(args.signing_key)
    public = bytes.fromhex(key["public"])
    key_id = hashlib.sha256(public).digest()[:16]
    signature_offset = SIZE + len(payload)
    values = (0x3155534B, SIZE, 1, fixed(args.release, 32), SIZE, len(payload), signature_offset, 64, 1, key_id)
    header = struct.pack(FORMAT, *values, b"\0" * 32, b"\0" * 16)
    digest = hashlib.sha256(header + payload).digest()
    header = struct.pack(FORMAT, *values, digest, b"\0" * 16)
    bundle = header + payload + sign(int(key["private"], 16), digest)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as stream:
        stream.write(bundle)
    print(f"KSU {args.release}: {len(bundle)} bytes -> {args.out}")

if __name__ == "__main__":
    main()
