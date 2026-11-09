#!/usr/bin/env python3
"""Verify the bounded reset predecessor bytes and fresh static objdump."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


HERE = Path(__file__).resolve().parent
EVIDENCE_PATH = HERE / "reset_predecessor_evidence.json"
RESET_EVIDENCE_PATH = HERE.parents[2] / "evidence/wave5/charge/full_reset_evidence.json"


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalized(line):
    return " ".join(line.lower().split())


def expected_line(row):
    # GNU objdump prints at most seven instruction bytes on a mnemonic line.
    raw = bytes.fromhex(row["bytes"])[:7]
    byte_tokens = " ".join(raw[index:index + 1].hex() for index in range(len(raw)))
    return normalized(f"{row['va'][2:]}: {byte_tokens} {row['instruction']}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True, type=Path)
    parser.add_argument("--objdump", default="objdump")
    args = parser.parse_args()

    evidence = json.loads(EVIDENCE_PATH.read_text())
    handoff_root = args.handoff.resolve() / "workspace"
    image = handoff_root / evidence["image"]["path"]
    base = int(evidence["image"]["image_base"], 16)

    for relative, expected_hash in evidence["source_sha256"].items():
        path = handoff_root / relative
        require(path.is_file(), f"source file missing: {relative}")
        require(sha256(path) == expected_hash, f"source hash mismatch: {relative}")

    reset_evidence = json.loads(RESET_EVIDENCE_PATH.read_text())
    require(sha256(RESET_EVIDENCE_PATH) ==
            evidence["predecessor"]["fallthrough_effect"]["source_evidence_sha256"],
            "existing eleven-store evidence hash mismatch")
    require(reset_evidence["image"]["sha256"] == evidence["image"]["sha256"],
            "reset evidence refers to a different image")
    stores = reset_evidence["stores"]
    require(len(stores) == evidence["predecessor"]["fallthrough_effect"]["store_count"],
            "existing reset store count mismatch")

    instructions = list(evidence["predecessor"]["instructions"])
    instructions.append(evidence["predecessor"]["add"])
    instructions.extend(stores)
    image_data = image.read_bytes()
    for row in instructions:
        va = int(row["va"], 16)
        raw = bytes.fromhex(row["bytes"])
        offset = va - base
        require(row["size"] == len(raw), f"size mismatch at {row['va']}")
        require(offset >= 0 and image_data[offset:offset + len(raw)] == raw,
                f"image bytes mismatch at {row['va']}")

    focused = evidence["predecessor"]["focused_objdump"]
    disassembly = subprocess.run(
        [
            args.objdump,
            "-D",
            "-Mintel",
            "-b",
            "binary",
            "-m",
            "i386",
            f"--adjust-vma={evidence['image']['image_base']}",
            f"--start-address={focused['start']}",
            f"--stop-address={focused['end_exclusive']}",
            evidence["image"]["path"],
        ],
        cwd=handoff_root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    saved_path = HERE.parents[2] / focused["saved_path"].removeprefix("continuation/")
    saved = saved_path.read_text()
    fresh_lines = {normalized(line) for line in disassembly.splitlines()}
    saved_lines = {normalized(line) for line in saved.splitlines()}
    for row in instructions:
        expected = expected_line(row)
        require(expected in fresh_lines, f"fresh objdump mismatch at {row['va']}")
        require(expected in saved_lines, f"saved objdump mismatch at {row['va']}")

    print("PASS: 5 source hashes, predecessor add/cmp/JNE, 11 reset stores, fresh objdump")
    print("UNRESOLVED: EAX provider/identity, entry reachability, loop and scheduling")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
