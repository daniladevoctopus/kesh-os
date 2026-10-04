#!/usr/bin/env python3
import argparse
import hashlib
import json
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path
from kpm_signing import generate_key, load_key, verify

V2_FORMAT = "<IIHH32s16s32s16s64sIIIHHQQQI8IIHHIHH16s32s"
V2_SIZE = struct.calcsize(V2_FORMAT)

def c_array(data):
    return ", ".join(f"0x{value:02x}" for value in data)

def emit_trust(key, output):
    public_key = bytes.fromhex(key["public"])
    key_id = hashlib.sha256(public_key).digest()[:16]
    text = "#include \"trusted_keys.h\"\n\n"
    text += "const kpm_trusted_key_t g_kpm_trusted_keys[] = {\n"
    text += "    { { " + c_array(key_id) + " }, { " + c_array(public_key) + " } }\n};\n\n"
    text += "const size_t g_kpm_trusted_key_count = 1;\n"
    Path(output).write_text(text, encoding="utf-8")

def inspect_package(path, public_key):
    data = path.read_bytes()
    if len(data) < V2_SIZE + 64:
        raise ValueError(f"{path}: truncated package")
    fields = struct.unpack_from(V2_FORMAT, data)
    if fields[0] != 0x0141454B or fields[2] != 2 or fields[1] != V2_SIZE:
        raise ValueError(f"{path}: signed KEA v2 required")
    signature_offset = fields[29]
    digest = fields[33]
    canonical = bytearray(data[:signature_offset])
    digest_offset = V2_SIZE - 32
    canonical[digest_offset:digest_offset + 32] = b"\0" * 32
    actual = hashlib.sha256(canonical).digest()
    if actual != digest or not verify(public_key, digest, data[signature_offset:]):
        raise ValueError(f"{path}: signature verification failed")
    decode = lambda value: value.split(b"\0", 1)[0].decode("utf-8")
    return {"id": path.stem, "name": decode(fields[4]), "version": decode(fields[5]),
            "author": decode(fields[6]), "category": decode(fields[7]), "description": decode(fields[8]),
            "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "key_id": fields[32].hex(), "filename": path.name,
            "download_url": f"/api/download?pkg={path.name}"}

def publish(source, repository, key):
    public_key = bytes.fromhex(key["public"])
    packages_dir = Path(repository) / "packages"
    packages_dir.mkdir(parents=True, exist_ok=True)
    packages = []
    for source_path in sorted(Path(source).glob("*.kea")):
        metadata = inspect_package(source_path, public_key)
        target_dir = packages_dir / metadata["id"] / metadata["version"]
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / source_path.name
        shutil.copyfile(source_path, target)
        metadata["download_url"] = f"/api/download?pkg={metadata['id']}&version={metadata['version']}"
        packages.append(metadata)
    index = {"schema": 2, "name": "KeshOS Package Registry", "updated_at": datetime.now(timezone.utc).isoformat(),
             "key_id": hashlib.sha256(public_key).digest()[:16].hex(), "packages": packages}
    payload = json.dumps(index, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()
    index["index_sha256"] = hashlib.sha256(payload).hexdigest()
    (Path(repository) / "index-v2.json").write_text(json.dumps(index, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    keygen = sub.add_parser("keygen")
    keygen.add_argument("--private", required=True)
    keygen.add_argument("--trust-header", required=True)
    release = sub.add_parser("publish")
    release.add_argument("--source", required=True)
    release.add_argument("--repository", required=True)
    release.add_argument("--key", required=True)
    args = parser.parse_args()
    if args.command == "keygen":
        emit_trust(generate_key(args.private), args.trust_header)
    else:
        publish(args.source, args.repository, load_key(args.key))

if __name__ == "__main__":
    main()
