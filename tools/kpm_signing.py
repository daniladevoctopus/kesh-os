import hashlib
import hmac
import json
import secrets
from pathlib import Path

P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
A = P - 3
B = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
G = (0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296,
     0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5)

def inv(value, modulus):
    return pow(value, modulus - 2, modulus)

def add(left, right):
    if left is None:
        return right
    if right is None:
        return left
    x1, y1 = left
    x2, y2 = right
    if x1 == x2 and (y1 + y2) % P == 0:
        return None
    if left == right:
        slope = (3 * x1 * x1 + A) * inv(2 * y1 % P, P) % P
    else:
        slope = (y2 - y1) * inv((x2 - x1) % P, P) % P
    x3 = (slope * slope - x1 - x2) % P
    return x3, (slope * (x1 - x3) - y1) % P

def multiply(value, point=G):
    result = None
    while value:
        if value & 1:
            result = add(result, point)
        point = add(point, point)
        value >>= 1
    return result

def public_bytes(private_value):
    x, y = multiply(private_value)
    return b"\x04" + x.to_bytes(32, "big") + y.to_bytes(32, "big")

def key_id(public_key):
    return hashlib.sha256(public_key).digest()[:16]

def deterministic_nonce(private_value, digest):
    x = private_value.to_bytes(32, "big")
    v = b"\x01" * 32
    k = b"\x00" * 32
    k = hmac.new(k, v + b"\x00" + x + digest, hashlib.sha256).digest()
    v = hmac.new(k, v, hashlib.sha256).digest()
    k = hmac.new(k, v + b"\x01" + x + digest, hashlib.sha256).digest()
    v = hmac.new(k, v, hashlib.sha256).digest()
    while True:
        v = hmac.new(k, v, hashlib.sha256).digest()
        candidate = int.from_bytes(v, "big")
        if 0 < candidate < N:
            return candidate
        k = hmac.new(k, v + b"\x00", hashlib.sha256).digest()
        v = hmac.new(k, v, hashlib.sha256).digest()

def sign(private_value, digest):
    nonce = deterministic_nonce(private_value, digest)
    r = multiply(nonce)[0] % N
    s = inv(nonce, N) * (int.from_bytes(digest, "big") + r * private_value) % N
    if s > N // 2:
        s = N - s
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")

def verify(public_key, digest, signature):
    if len(public_key) != 65 or public_key[0] != 4 or len(signature) != 64:
        return False
    point = (int.from_bytes(public_key[1:33], "big"), int.from_bytes(public_key[33:], "big"))
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    if not (0 < r < N and 0 < s < N):
        return False
    w = inv(s, N)
    check = add(multiply(int.from_bytes(digest, "big") * w % N), multiply(r * w % N, point))
    return check is not None and check[0] % N == r

def generate_key(path):
    private_value = secrets.randbelow(N - 1) + 1
    public_key = public_bytes(private_value)
    payload = {"algorithm": "ECDSA-P256-SHA256", "key_id": key_id(public_key).hex(),
               "private": private_value.to_bytes(32, "big").hex(), "public": public_key.hex()}
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload

def load_key(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))
