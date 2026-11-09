#!/usr/bin/env python3
"""Standalone static byte/hash/constant gate for the W8 movement body."""

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path


ANCHORS = Path(__file__).with_name("anchors.json")


def sha256_file(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def verify(handoff: Path) -> dict:
    anchors = json.loads(ANCHORS.read_text())
    source = anchors["source"]
    data = (handoff / source["path"]).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != source["sha256"] or len(data) != source["size"]:
        raise ValueError("restored nl.bin hash/size mismatch")
    archive = handoff / source["archive_path"]
    if sha256_file(archive) != source["archive_sha256"]:
        raise ValueError("source archive hash mismatch")
    with zipfile.ZipFile(archive) as bundle:
        with bundle.open(source["archive_member"]) as member:
            if hashlib.file_digest(member, "sha256").hexdigest() != digest:
                raise ValueError("archive nl.bin differs from restored source")

    base = int(source["image_base"], 16)

    def read(va: int, size: int) -> bytes:
        offset = va - base
        if offset < 0 or offset + size > len(data):
            raise ValueError(f"VA {va:#x} is outside the source snapshot")
        return data[offset:offset + size]

    checked = []
    for region in anchors["regions"]:
        expected = bytes.fromhex(region["bytes"])
        if len(expected) != region["size"]:
            raise ValueError("anchor size mismatch")
        if hashlib.sha256(expected).hexdigest() != region["sha256"]:
            raise ValueError("anchor hash mismatch")
        va = int(region["va"], 16)
        if read(va, len(expected)) != expected:
            raise ValueError(f"byte mismatch at {va:#x}")
        checked.append({"label": region["label"], "va": region["va"],
                        "size": len(expected), "sha256": region["sha256"]})

    def immediate(va: int, opcode: str) -> int:
        prefix = bytes.fromhex(opcode)
        if read(va, len(prefix)) != prefix:
            raise ValueError(f"unexpected arithmetic opcode at {va:#x}")
        return struct.unpack("<I", read(va + len(prefix), 4))[0]

    def rol32(value: int, count: int) -> int:
        return ((value << count) | (value >> (32 - count))) & 0xffffffff

    # Evaluate only pinned immediate arithmetic, not original execution/emulation.
    guard_1 = immediate(0x414A5160, "b8")
    guard_1 = (guard_1 - immediate(0x414A5165, "2d")) & 0xffffffff
    guard_1 = (guard_1 - immediate(0x414A516A, "2d")) & 0xffffffff
    guard_1 ^= immediate(0x414A5172, "35")
    guard_1 = (guard_1 - immediate(0x414A5177, "2d")) & 0xffffffff
    guard_1 = rol32(guard_1, 2)

    guard_2 = immediate(0x414A520F, "b8")
    guard_2 = (guard_2 - immediate(0x414A5217, "2d")) & 0xffffffff
    guard_2 = (guard_2 - immediate(0x414A521C, "2d")) & 0xffffffff
    guard_2 = rol32(guard_2, 5)
    guard_2 = (guard_2 - immediate(0x414A5227, "2d")) & 0xffffffff
    guard_2 ^= immediate(0x414A522C, "35")

    object_1 = immediate(0x414A51B4, "b8") ^ immediate(0x414A51B9, "35")
    object_1 = rol32(rol32(object_1, 16), 21)
    object_1 = (object_1 + immediate(0x414A51C7, "05")) & 0xffffffff
    object_1 = (object_1 - immediate(0x414A51CC, "2d")) & 0xffffffff
    object_1 ^= immediate(0x414A51D1, "35")

    object_2 = immediate(0x414A526F, "b8")
    object_2 = (object_2 + immediate(0x414A5274, "05")) & 0xffffffff
    object_2 = (object_2 - immediate(0x414A5279, "2d")) & 0xffffffff
    object_2 = (object_2 + immediate(0x414A527E, "05")) & 0xffffffff
    object_2 = (object_2 - immediate(0x414A5283, "2d")) & 0xffffffff
    object_2 = rol32(object_2, 21) ^ immediate(0x414A528B, "35")
    constants = (guard_1, guard_2, object_1, object_2)
    if constants != (0x20, 0x20, 0x28f3bfb0, 0x28f2e100):
        raise ValueError(f"unexpected decoded constants: {constants}")
    if any(base <= obj < base + len(data) for obj in (object_1, object_2)):
        raise ValueError("expected both object literals outside this snapshot")

    def relative_target(va: int, opcode: str, width: int, target: int) -> dict:
        prefix = bytes.fromhex(opcode)
        if read(va, len(prefix)) != prefix:
            raise ValueError(f"unexpected branch opcode at {va:#x}")
        displacement = struct.unpack("<b" if width == 1 else "<i",
                                     read(va + len(prefix), width))[0]
        actual = va + len(prefix) + width + displacement
        if actual != target:
            raise ValueError(f"unexpected branch target at {va:#x}: {actual:#x}")
        return {"site": f"{va:#010x}", "target": f"{actual:#010x}"}

    branches = [relative_target(*entry) for entry in (
        (0x4148CC4D, "0f84", 4, 0x4148D7B9),
        (0x4148CC56, "0f84", 4, 0x4148D7B9),
        (0x4148CC63, "75", 1, 0x4148CC8C),
        (0x4148CC7C, "e8", 4, 0x414A5130),
        (0x4148CC86, "0f84", 4, 0x4148D7B7),
        (0x414A518B, "74", 1, 0x414A51B4),
        (0x414A51E8, "0f85", 4, 0x414A52CA),
        (0x414A523A, "74", 1, 0x414A526F),
        (0x414A5290, "eb", 1, 0x414A5292),
        (0x414A52D0, "e8", 4, 0x41DA7C5C),
    )]
    if read(0x4206CDF4, 4) != bytes.fromhex("0000803e"):
        raise ValueError("MULSS factor is not binary32 0.25")
    if read(0x4205A540, 16) != bytes.fromhex("ffffff7f" * 4):
        raise ValueError("ANDPS mask mismatch")

    return {
        "result": "passed", "source": source, "pins": checked,
        "branches": branches,
        "guard_comparison_values": [f"{value:#x}" for value in constants[:2]],
        "distinct_external_ecx_literals": [f"{value:#x}" for value in constants[2:]],
        "original_loaded_run_or_emulated": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.handoff), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
