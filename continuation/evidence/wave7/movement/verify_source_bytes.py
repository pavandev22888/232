#!/usr/bin/env python3
"""Exact-byte gate for the W7 bounded caller-to-predicate slice."""

import argparse
import hashlib
import json
import struct
from pathlib import Path


SOURCE_RELATIVE = Path("workspace/work/nl_handoff_2026-11-06/nl.bin")
SOURCE_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
IMAGE_BASE = 0x412A0000
PINS = (
    ("caller selector, guard, float push and AL consumer", 0x4148CC44,
     "0fb7043031db83f81f0f84660b000083f8400f845d0b0000"
     "f605b8525042017527660f6e05dc525042c745f0ffffffff"
     "83ec04660f7e0424e8af84010083c40484c00f842b0b0000"),
    ("predicate entry and frame allocation", 0x414A5130,
     "5383ec0ca10027264231e089442408"),
    ("predicate float load, mask, comparison and SETBE", 0x414A52B7,
     "f30f104c24140f540d40a505420f2ec80f96c3"),
    ("predicate AL return path", 0x414A52CA,
     "8b4c240831e1e88729900089d883c40c5bc3"),
    ("caller entry frame", 0x4148CAE0,
     "5589e553575681ecac0200008965e4"),
)


def verify(handoff: Path) -> dict:
    source = handoff / SOURCE_RELATIVE
    data = source.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError(f"source SHA-256 mismatch: {digest}")

    checked = []
    for label, va, expected_hex in PINS:
        start = va - IMAGE_BASE
        expected = bytes.fromhex(expected_hex)
        actual = data[start:start + len(expected)]
        if actual != expected:
            raise ValueError(
                f"byte mismatch at {va:#010x}: expected {expected.hex()}, "
                f"got {actual.hex()}"
            )
        checked.append({
            "label": label,
            "va": f"{va:#010x}",
            "length": len(expected),
            "bytes": expected.hex(),
        })

    caller = bytes.fromhex(PINS[0][2])
    call_offset = 0x4148CC7C - PINS[0][1]
    if caller[call_offset] != 0xE8:
        raise ValueError("expected direct CALL opcode at 0x4148cc7c")
    displacement = struct.unpack_from("<i", caller, call_offset + 1)[0]
    call_target = 0x4148CC7C + 5 + displacement
    if call_target != 0x414A5130:
        raise ValueError(f"unexpected call target: {call_target:#010x}")

    def relative_target(va: int, width: int) -> int:
        offset = va - PINS[0][1]
        if width == 1:
            displacement = struct.unpack_from("<b", caller, offset + 1)[0]
            instruction_size = 2
        else:
            displacement = struct.unpack_from("<i", caller, offset + 2)[0]
            instruction_size = 6
        return va + instruction_size + displacement

    branch_targets = {
        "selector_0x1f": relative_target(0x4148CC4D, 4),
        "selector_0x40": relative_target(0x4148CC56, 4),
        "guard_bit_set": relative_target(0x4148CC63, 1),
        "predicate_al_zero": relative_target(0x4148CC86, 4),
    }
    expected_targets = {
        "selector_0x1f": 0x4148D7B9,
        "selector_0x40": 0x4148D7B9,
        "guard_bit_set": 0x4148CC8C,
        "predicate_al_zero": 0x4148D7B7,
    }
    if branch_targets != expected_targets:
        raise ValueError(f"unexpected caller branch targets: {branch_targets}")

    return {
        "result": "passed",
        "source": str(SOURCE_RELATIVE),
        "source_sha256": digest,
        "image_base": f"{IMAGE_BASE:#010x}",
        "pins": checked,
        "direct_call": {
            "site": "0x4148cc7c",
            "target": f"{call_target:#010x}",
            "displacement": f"{displacement:+#x}",
        },
        "caller_branch_targets": {
            name: f"{target:#010x}" for name, target in branch_targets.items()
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.handoff), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
