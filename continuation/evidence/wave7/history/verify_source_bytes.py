#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path

NL_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
DLL_SHA256 = "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c"
RAW_BASE = 0x412A0000
RESOURCE_OFFSET = 0x629B10
RESOURCE_SIZE = 0x3501000
RESOLVER_SHA256 = "b8d8206f38ea7844a1db06850dd6d6a098b2fc0e989946a045ca26ecff200a92"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def source_bytes(image, va, size):
    offset = va - RAW_BASE
    data = image[offset:offset + size]
    if offset < 0 or len(data) != size:
        raise SystemExit(f"FAIL source range {va:#x}+{size:#x}")
    return data


def check_anchor(image, va, expected):
    actual = source_bytes(image, va, len(expected))
    if actual != expected:
        raise SystemExit(f"FAIL bytes at {va:#x}: {actual.hex()} != {expected.hex()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True)
    args = parser.parse_args()
    handoff = Path(args.handoff)
    image_path = handoff / "workspace/work/nl_handoff_2026-11-06/nl.bin"
    dll_path = handoff / "workspace/input/dll/neverlose.dll"
    image = image_path.read_bytes()
    dll_hash = hashlib.sha256(dll_path.read_bytes()).hexdigest()

    if sha256(image) != NL_SHA256:
        raise SystemExit("FAIL nl.bin SHA-256")
    if dll_hash != DLL_SHA256:
        raise SystemExit("FAIL neverlose.dll SHA-256")
    if len(image) != RESOURCE_SIZE:
        raise SystemExit("FAIL nl.bin size")
    dll = dll_path.read_bytes()
    if dll[RESOURCE_OFFSET:RESOURCE_OFFSET + RESOURCE_SIZE] != image:
        raise SystemExit("FAIL DLL resource bytes differ from nl.bin")

    resolver = source_bytes(image, 0x413F88F0, 0xC0)
    if sha256(resolver) != RESOLVER_SHA256:
        raise SystemExit("FAIL resolver body bytes/hash")
    check_anchor(image, 0x413F70F3, bytes.fromhex("e8f8170000"))
    check_anchor(image, 0x413F70F8, bytes.fromhex("89c3"))
    check_anchor(image, 0x413F70FD, bytes.fromhex("e82e987700"))
    check_anchor(image, 0x413F7102, bytes.fromhex("8b55d8"))
    check_anchor(image, 0x413F710A, bytes.fromhex("f30f1103"))
    check_anchor(image, 0x413F88FE, bytes.fromhex("770c"))
    check_anchor(image, 0x413F8904, bytes.fromhex("e887afffff"))
    check_anchor(image, 0x413F8914, bytes.fromhex("897e0c0f44f84f21fb"))
    check_anchor(image, 0x413F891A, bytes.fromhex("4f21fb"))
    check_anchor(image, 0x413F8925, bytes.fromhex("751b"))
    check_anchor(image, 0x413F8927, bytes.fromhex("6a2068c0f00000e879f29a00"))
    check_anchor(image, 0x413F8945, bytes.fromhex("0f294030"))
    check_anchor(image, 0x413F8975, bytes.fromhex("0f280dd0af05420f1188ac000000"))
    check_anchor(image, 0x413F899A, bytes.fromhex("897e0cff4610"))
    check_anchor(image, 0x413F89A9, bytes.fromhex("8b04885e5f5bc3"))
    check_anchor(image, 0x4205AFD0, bytes.fromhex("0000000000000000ffffffffffffffff"))

    print("PASS source DLL resource bytes equal nl.bin")
    print(f"PASS nl.bin sha256={NL_SHA256} size={len(image)}")
    print(f"PASS neverlose.dll sha256={dll_hash}")
    print(f"PASS resolver VA=0x413f88f0 length=0xc0 sha256={RESOLVER_SHA256}")
    print("PASS resolver cursor/index (DEC EDI; AND EBX,EDI; store EDI), init/return, and caller/consumer anchors")
    print("PASS initializer vector VA=0x4205afd0 bytes=0000000000000000ffffffffffffffff")


if __name__ == "__main__":
    main()
