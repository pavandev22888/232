#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path

NL_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
DLL_SHA256 = "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c"
RAW_BASE = 0x412A0000
RESOURCE_OFFSET = 0x629B10
RESOURCE_SIZE = 0x3501000


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_anchor(image, va, expected):
    offset = va - RAW_BASE
    actual = image[offset:offset + len(expected)]
    if actual != expected:
        raise SystemExit(
            f"FAIL VA 0x{va:08x} raw+0x{offset:x}: {actual.hex()} != {expected.hex()}"
        )
    return offset


def check_walk(image, path, begin, end):
    walk = json.loads(Path(path).read_text())
    matches = 0
    for instruction in walk["instructions"]:
        va = int(instruction["va"], 16)
        size = int(instruction["size"])
        if begin <= va < end:
            if va + size > end:
                raise SystemExit(f"FAIL partial instruction at 0x{va:08x}")
            actual = image[va - RAW_BASE:va - RAW_BASE + size]
            if actual.hex() != instruction["bytes"]:
                raise SystemExit(f"FAIL saved walk mismatch at 0x{va:08x}")
            matches += 1
    if not matches:
        raise SystemExit(f"FAIL no saved instruction anchors in {path}")
    return matches


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True)
    args = parser.parse_args()
    handoff = Path(args.handoff)
    image_path = handoff / "workspace/work/nl_handoff_2026-11-06/nl.bin"
    dll_path = handoff / "workspace/input/dll/neverlose.dll"
    producer_walk = handoff / "workspace/orchestration/v3/history_record_producer_walk.json"
    constructor_walk = handoff / "workspace/orchestration/v3/history_record_constructor_walk.json"
    image = image_path.read_bytes()

    image_hash = sha256_bytes(image)
    dll_hash = sha256_file(dll_path)
    if image_hash != NL_SHA256:
        raise SystemExit("FAIL nl.bin SHA-256")
    if dll_hash != DLL_SHA256:
        raise SystemExit("FAIL neverlose.dll SHA-256")
    if len(image) != RESOURCE_SIZE:
        raise SystemExit("FAIL nl.bin size")
    with dll_path.open("rb") as dll:
        dll.seek(RESOURCE_OFFSET)
        resource = dll.read(RESOURCE_SIZE)
    if len(resource) != RESOURCE_SIZE or resource != image:
        raise SystemExit("FAIL DLL resource bytes differ from nl.bin")

    anchors = [
        (0x413F6DE0, bytes.fromhex("5589e553575683ec4489cf8b5508")),
        (0x413F6E1A, bytes.fromhex("ff5028")),
        (0x413F6E20, bytes.fromhex("e8fbccf4ff")),
        (0x413F7054, bytes.fromhex("8b03")),
        (0x413F7062, bytes.fromhex("ffd0")),
        (0x413F7068, bytes.fromhex("e8b3caf4ff")),
        (0x413F70F3, bytes.fromhex("e8f8170000")),
        (0x413F70FD, bytes.fromhex("e82e987700")),
        (0x413F7102, bytes.fromhex("8b55d8")),
        (0x413F7201, bytes.fromhex("85c07465")),
        (0x413F723D, bytes.fromhex("e89e957700")),
        (0x413F88F0, bytes.fromhex("53575689ce8b41088b491041")),
        (0x413F8904, bytes.fromhex("e887afffff")),
        (0x413F892E, bytes.fromhex("e879f29a00")),
    ]
    offsets = [check_anchor(image, va, raw) for va, raw in anchors]
    producer = check_walk(image, producer_walk, 0x413F6DE0, 0x413F6E40)
    resolver = check_walk(image, producer_walk, 0x413F7054, 0x413F7078)
    consumer = check_walk(image, producer_walk, 0x413F7102, 0x413F7201)
    constructor = check_walk(image, constructor_walk, 0x413F88F0, 0x413F8967)

    print("PASS source DLL resource bytes equal nl.bin")
    print(f"PASS nl.bin sha256={image_hash} size={len(image)}")
    print(f"PASS neverlose.dll sha256={dll_hash}")
    print(f"PASS {len(anchors)} raw-byte anchors; offsets=" + ",".join(hex(x) for x in offsets))
    print(
        "PASS saved walks match fresh bytes: "
        f"producer={producer}, resolver={resolver}, consumer={consumer}, constructor={constructor}"
    )


if __name__ == "__main__":
    main()
