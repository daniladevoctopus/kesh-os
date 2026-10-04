import hashlib
import struct
import zlib

P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
A = P - 3
N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
G = (0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296,
     0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5)
PUBLIC_KEY = bytes.fromhex("0424d3bb0912b860e5b1b9c860fe5290b00cb287fbda2549fe34fd4e8e132257ced23ba2cf2cc43afca1abe7901587a201eaa1506a36bac05a1d210df9608138e0")
KEY_ID = hashlib.sha256(PUBLIC_KEY).digest()[:16]
FORMAT = "<IIHH32s16s32s16s64sIIIHHQQQI8IIHHIHH16s32s"
SIZE = struct.calcsize(FORMAT)

def inverse(value, modulus):
    return pow(value, modulus - 2, modulus)

def point_add(left, right):
    if left is None:
        return right
    if right is None:
        return left
    x1, y1 = left
    x2, y2 = right
    if x1 == x2 and (y1 + y2) % P == 0:
        return None
    slope = ((3 * x1 * x1 + A) * inverse(2 * y1 % P, P) if left == right
             else (y2 - y1) * inverse((x2 - x1) % P, P)) % P
    x3 = (slope * slope - x1 - x2) % P
    return x3, (slope * (x1 - x3) - y1) % P

def multiply(value, point):
    result = None
    while value:
        if value & 1:
            result = point_add(result, point)
        point = point_add(point, point)
        value >>= 1
    return result

def signature_valid(digest, signature):
    if len(signature) != 64:
        return False
    public = (int.from_bytes(PUBLIC_KEY[1:33], "big"), int.from_bytes(PUBLIC_KEY[33:], "big"))
    r, s = int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big")
    if not (0 < r < N and 0 < s < N):
        return False
    w = inverse(s, N)
    result = point_add(multiply(int.from_bytes(digest, "big") * w % N, G), multiply(r * w % N, public))
    return result is not None and result[0] % N == r

def verify_package(data):
    if len(data) < SIZE + 64:
        return None
    fields = struct.unpack_from(FORMAT, data)
    if fields[0] != 0x0141454B or fields[1] != SIZE or fields[2] != 2 or not (fields[3] & 1):
        return None
    icon_offset, icon_size, elf_offset, elf_size = fields[10], fields[11], fields[14], fields[15]
    signature_offset, signature_size, algorithm = fields[29], fields[30], fields[31]
    if icon_offset + icon_size > len(data) or elf_offset + elf_size > signature_offset or signature_offset + signature_size != len(data):
        return None
    if signature_size != 64 or algorithm != 1 or fields[32] != KEY_ID:
        return None
    if zlib.crc32(data[elf_offset:elf_offset + elf_size]) & 0xffffffff != fields[17]:
        return None
    canonical = bytearray(data[:signature_offset])
    canonical[SIZE - 32:SIZE] = b"\0" * 32
    digest = hashlib.sha256(canonical).digest()
    if digest != fields[33] or not signature_valid(digest, data[signature_offset:]):
        return None
    decode = lambda value: value.split(b"\0", 1)[0].decode("utf-8", errors="replace")
    return {"name": decode(fields[4]), "version": decode(fields[5]), "author": decode(fields[6]),
            "category": decode(fields[7]), "description": decode(fields[8]), "entry_point": hex(fields[16]),
            "crc": hex(fields[17]), "size": len(data), "key_id": fields[32].hex()}
