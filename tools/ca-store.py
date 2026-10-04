import argparse
import base64
import hashlib
import os
import struct
from kpm_signing import load_key, sign

FORMAT = "<IHHIHHHH16s32s64s"
SIZE = struct.calcsize(FORMAT)

def tlv(data, offset):
    if offset + 2 > len(data):
        raise ValueError("truncated DER")
    tag = data[offset]
    first = data[offset + 1]
    if first < 128:
        length = first
        content = offset + 2
    else:
        count = first & 127
        if count == 0 or count > 4 or offset + 2 + count > len(data):
            raise ValueError("invalid DER length")
        length = int.from_bytes(data[offset + 2:offset + 2 + count], "big")
        content = offset + 2 + count
    end = content + length
    if end > len(data):
        raise ValueError("truncated DER value")
    return tag, content, end, end

def certificate_bytes(path):
    data = open(path, "rb").read()
    if data.startswith(b"-----BEGIN CERTIFICATE-----"):
        body = data.split(b"-----BEGIN CERTIFICATE-----", 1)[1].split(b"-----END CERTIFICATE-----", 1)[0]
        data = base64.b64decode(body)
    return data

def rsa_anchor(data):
    tag, cert_start, cert_end, _ = tlv(data, 0)
    if tag != 0x30 or cert_end != len(data):
        raise ValueError("invalid certificate")
    tag, tbs_start, tbs_end, _ = tlv(data, cert_start)
    if tag != 0x30:
        raise ValueError("invalid TBSCertificate")
    cursor = tbs_start
    if data[cursor] == 0xA0:
        cursor = tlv(data, cursor)[3]
    for _ in range(4):
        cursor = tlv(data, cursor)[3]
    subject_offset = cursor
    tag, _, _, cursor = tlv(data, cursor)
    if tag != 0x30:
        raise ValueError("invalid subject")
    subject = data[subject_offset:cursor]
    tag, spki_start, _, _ = tlv(data, cursor)
    if tag != 0x30:
        raise ValueError("invalid SubjectPublicKeyInfo")
    algorithm_end = tlv(data, spki_start)[3]
    tag, bit_start, bit_end, _ = tlv(data, algorithm_end)
    if tag != 0x03 or bit_start >= bit_end or data[bit_start] != 0:
        raise ValueError("invalid RSA public key")
    tag, rsa_start, _, _ = tlv(data, bit_start + 1)
    if tag != 0x30:
        raise ValueError("unsupported public key")
    tag, n_start, n_end, cursor = tlv(data, rsa_start)
    if tag != 0x02:
        raise ValueError("invalid RSA modulus")
    tag, e_start, e_end, _ = tlv(data, cursor)
    if tag != 0x02:
        raise ValueError("invalid RSA exponent")
    modulus = data[n_start:n_end].lstrip(b"\0")
    exponent = data[e_start:e_end].lstrip(b"\0")
    if not subject or not modulus or not exponent or len(subject) > 256 or len(modulus) > 512 or len(exponent) > 8:
        raise ValueError("certificate key is outside KeshOS limits")
    return subject, modulus, exponent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--certificate", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--generation", required=True, type=int)
    parser.add_argument("--signing-key", default=os.environ.get("KPM_SIGNING_KEY"))
    args = parser.parse_args()
    if not args.signing_key or args.generation <= 0 or args.generation > 0xffffffff:
        parser.error("a signing key and positive 32-bit generation are required")
    dn, modulus, exponent = rsa_anchor(certificate_bytes(args.certificate))
    key = load_key(args.signing_key)
    public = bytes.fromhex(key["public"])
    key_id = hashlib.sha256(public).digest()[:16]
    values = (0x5341434B, 1, SIZE, args.generation, len(dn), len(modulus), len(exponent), 1, key_id)
    payload = dn + modulus + exponent
    empty = struct.pack(FORMAT, *values, b"\0" * 32, b"\0" * 64)
    digest = hashlib.sha256(empty + payload).digest()
    image = struct.pack(FORMAT, *values, digest, sign(int(key["private"], 16), digest)) + payload
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as stream:
        stream.write(image)
    print(f"KeshOS CA store generation {args.generation}: {len(image)} bytes -> {args.out}")

if __name__ == "__main__":
    main()
