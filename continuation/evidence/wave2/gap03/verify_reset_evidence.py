#!/usr/bin/env python3
"""Check immutable reset context, shared operands, and fresh static decoding."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


EVIDENCE = Path(__file__).with_name("reset_slice_evidence.json")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def normalized(instruction: str) -> str:
    return re.sub(r"\s+", "", instruction).lower()


def fresh_decode(image: Path, base: int, start: int, end: int) -> dict:
    result = subprocess.run(
        [
            "objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
            "--insn-width=16", f"--adjust-vma={base:#x}",
            f"--start-address={start:#x}", f"--stop-address={end:#x}",
            str(image),
        ],
        check=True, capture_output=True, text=True,
    )
    rows = {}
    for line in result.stdout.splitlines():
        match = re.match(
            r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)\s*(\S.*)$", line
        )
        if match:
            va = int(match[1], 16)
            require(va not in rows, f"duplicate objdump row {va:#x}")
            rows[va] = (bytes.fromhex(match[2]), normalized(match[3]))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--handoff", type=Path, required=True,
        help="RAGEBOT_HANDOFF_V10 directory containing workspace/",
    )
    args = parser.parse_args()
    workspace = args.handoff.resolve() / "workspace"
    evidence = json.loads(EVIDENCE.read_text(encoding="utf-8"))

    for relative, expected in evidence["source_sha256"].items():
        path = workspace / relative
        require(path.is_file(), f"missing source {relative}")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected,
                f"source hash mismatch: {relative}")

    image_info = evidence["image"]
    image_path = workspace / image_info["path"]
    image = image_path.read_bytes()
    require(hashlib.sha256(image).hexdigest() == image_info["sha256"],
            "original image hash mismatch")
    base = int(image_info["image_base"], 16)
    anchors = {}
    instruction_count = 0
    for section in evidence["sections"]:
        walk = json.loads((workspace / section["walk"]).read_text(encoding="utf-8"))
        saved = {row["va"]: row for row in walk["instructions"]}
        start = int(section["start"], 16)
        end = int(section["end_exclusive"], 16)
        decoded = fresh_decode(image_path, base, start, end)
        expected_addresses = set()
        cursor = start
        for row in section["instructions"]:
            va = int(row["va"], 16)
            expected_bytes = bytes.fromhex(row["bytes"])
            require(va == cursor and row["size"] == len(expected_bytes),
                    f"noncontiguous or wrong-size anchor: {row['va']}")
            require(saved.get(row["va"]) == row,
                    f"saved walk mismatch: {row['va']}")
            offset = va - base
            require(offset >= 0 and
                    image[offset:offset + len(expected_bytes)] == expected_bytes,
                    f"original bytes mismatch: {row['va']}")
            require(decoded.get(va) ==
                    (expected_bytes, normalized(row["instruction"])),
                    f"fresh objdump mismatch: {row['va']}")
            require(row["va"] not in anchors, f"duplicate anchor: {row['va']}")
            anchors[row["va"]] = expected_bytes
            expected_addresses.add(va)
            cursor += len(expected_bytes)
            instruction_count += 1
        require(cursor == end and set(decoded) == expected_addresses,
                f"incomplete context: {section['name']}")

    for binding in evidence["projection"]:
        address = int(binding["address"], 16).to_bytes(4, "little")
        width = binding["width"]
        require(width in (1, 4) and binding["value"] == 0,
                f"invalid zero-store projection: {binding['member']}")
        store = (b"\xc6\x05" if width == 1 else b"\xc7\x05")
        require(anchors[binding["reset_va"]] == store + address + bytes(width),
                f"reset address/width mismatch: {binding['member']}")
        consumer = (b"\x80\x3d" + address + b"\x00" if width == 1
                    else b"\x3b\x0d" + address)
        require(anchors[binding["consumer_va"]] == consumer,
                f"small-gate shared address mismatch: {binding['member']}")
        if "set_va" in binding:
            require(anchors[binding["set_va"]] == b"\xc6\x05" + address + b"\x01",
                    "set/reset latch address mismatch")

    print(f"PASS: {len(evidence['source_sha256'])} immutable source hashes, "
          f"{instruction_count} byte anchors/fresh objdump instructions, "
          f"{len(evidence['sections'])} complete static contexts, "
          f"{len(evidence['projection'])} same-address reset/consumer bindings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
