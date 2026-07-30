#!/usr/bin/env python3
"""Build deterministic RFC 8554 LMS H5/W4 and H5/W8 test capsules.

This is a reproducible demo signer, not an SP 800-208-compliant production
signer. Production signing state belongs in a non-exporting cryptographic
module and each LM-OTS leaf may be released at most once.
"""

from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path

LMS_TYPE = 5
HEIGHT = 5
MANIFEST = Path("tests/vectors/cutover_manifest.bin")
OUTPUT = Path("tests/vectors")


@dataclass(frozen=True)
class Profile:
    name: str
    ots_type: int
    width: int
    p: int
    checksum_shift: int

    @property
    def signature_bytes(self) -> int:
        return 12 + 32 * (self.p + 1) + 32 * HEIGHT


PROFILES = (
    Profile("lms_w4", 3, 4, 67, 4),
    Profile("lms_w8", 4, 8, 34, 0),
)


def h(*parts: bytes) -> bytes:
    digest = hashlib.sha256()
    for part in parts:
        digest.update(part)
    return digest.digest()


def u16(value: int) -> bytes:
    return struct.pack(">H", value)


def u32(value: int) -> bytes:
    return struct.pack(">I", value)


def coefficients(data: bytes, width: int) -> list[int]:
    output: list[int] = []
    mask = (1 << width) - 1
    for value in data:
        for shift in range(8 - width, -1, -width):
            output.append((value >> shift) & mask)
    return output


def private_element(identifier: bytes, seed: bytes, q: int, i: int) -> bytes:
    return h(identifier, u32(q), u16(i), b"\xff", seed)


def chain(identifier: bytes, q: int, i: int, start: int, stop: int, x: bytes) -> bytes:
    value = x
    for iteration in range(start, stop):
        value = h(identifier, u32(q), u16(i), bytes([iteration]), value)
    return value


def ots_public(
    profile: Profile,
    identifier: bytes,
    seed: bytes,
    q: int,
) -> bytes:
    digest = hashlib.sha256()
    digest.update(identifier)
    digest.update(u32(q))
    digest.update(b"\x80\x80")
    chain_limit = (1 << profile.width) - 1
    for i in range(profile.p):
        value = private_element(identifier, seed, q, i)
        digest.update(chain(identifier, q, i, 0, chain_limit, value))
    return digest.digest()


def build_tree(
    profile: Profile,
    identifier: bytes,
    seed: bytes,
) -> list[bytes]:
    tree = [b""] * (1 << (HEIGHT + 1))
    for q in range(1 << HEIGHT):
        node = (1 << HEIGHT) + q
        tree[node] = h(
            identifier,
            u32(node),
            b"\x82\x82",
            ots_public(profile, identifier, seed, q),
        )
    for node in range((1 << HEIGHT) - 1, 0, -1):
        tree[node] = h(
            identifier,
            u32(node),
            b"\x83\x83",
            tree[node * 2],
            tree[node * 2 + 1],
        )
    return tree


def sign(
    profile: Profile,
    identifier: bytes,
    seed: bytes,
    tree: list[bytes],
    q: int,
    message: bytes,
) -> bytes:
    randomizer = h(b"NanoPQ LMS C", bytes([profile.width]), seed, u32(q))
    message_hash = h(
        identifier,
        u32(q),
        b"\x81\x81",
        randomizer,
        message,
    )
    message_coefficients = coefficients(message_hash, profile.width)
    chain_limit = (1 << profile.width) - 1
    checksum = sum(chain_limit - value for value in message_coefficients)
    checksum <<= profile.checksum_shift
    all_coefficients = coefficients(
        message_hash + u16(checksum),
        profile.width,
    )[: profile.p]
    signature = bytearray(
        u32(q) + u32(profile.ots_type) + randomizer
    )
    for i, coefficient in enumerate(all_coefficients):
        value = private_element(identifier, seed, q, i)
        signature.extend(
            chain(identifier, q, i, 0, coefficient, value)
        )
    signature.extend(u32(LMS_TYPE))
    node = (1 << HEIGHT) + q
    for _ in range(HEIGHT):
        signature.extend(tree[node ^ 1])
        node //= 2
    assert len(signature) == profile.signature_bytes
    return bytes(signature)


def build_profile(
    profile: Profile,
    message: bytes,
    seed: bytes,
    identifier: bytes,
) -> tuple[bytes, bytes]:
    tree = build_tree(profile, identifier, seed)
    public_key = (
        u32(LMS_TYPE)
        + u32(profile.ots_type)
        + identifier
        + tree[1]
    )
    signature = sign(profile, identifier, seed, tree, 0, message)
    return public_key, signature


def main() -> None:
    message = MANIFEST.read_bytes()
    if len(message) != 48:
        raise SystemExit("cutover manifest must be exactly 48 bytes")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for profile in PROFILES:
        seed = h(
            b"NanoPQ LMS demo seed 2026-07-30",
            bytes([profile.width]),
        )
        identifier = h(
            b"NanoPQ LMS identifier",
            bytes([profile.width]),
        )[:16]
        public_key, signature = build_profile(
            profile,
            message,
            seed,
            identifier,
        )
        (OUTPUT / f"{profile.name}_public.bin").write_bytes(public_key)
        (OUTPUT / f"{profile.name}_signature.bin").write_bytes(signature)
        print(
            f"wrote {profile.name} public={len(public_key)} "
            f"signature={len(signature)} using leaf q=0 of 32"
        )
        if profile.width == 4:
            (OUTPUT / "lms_public.bin").write_bytes(public_key)
            (OUTPUT / "lms_signature.bin").write_bytes(signature)


if __name__ == "__main__":
    main()
