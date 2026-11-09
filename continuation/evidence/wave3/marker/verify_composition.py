#!/usr/bin/env python3
"""Read-only pinned byte/fresh-decode checks; never loads or executes the image."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def decode(image_path, base, start, end):
    output = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={base:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(image_path)],
        capture_output=True, text=True, check=True, timeout=30)
    rows = []
    for line in output.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            rows.append({"va": hex(int(match[1], 16)),
                         "bytes": "".join(match[2].split()),
                         "instruction": " ".join(match[3].split()).lower()})
    return rows


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    require(args.handoff.is_absolute(), "--handoff must be absolute")
    handoff = args.handoff.resolve()
    proof = json.loads((here / "composition.json").read_text())
    for pinned in proof["pinned_repository_evidence"]:
        require(sha256((repo / pinned["path"]).read_bytes()) == pinned["sha256"],
                f"pinned evidence: {pinned['path']}")

    subprocess.run([sys.executable, "-B", str(repo / proof["reused_helper_verifier"]),
                    "--handoff", str(handoff)], check=True, timeout=120)
    helper = json.loads((repo / proof["reused_helper_proof"]).read_text())
    producer = json.loads((repo / proof["reused_producer_proof"]).read_text())
    image_path = handoff / helper["image"]["path"]
    image = image_path.read_bytes()
    require(sha256(image) == helper["image"]["sha256"] == producer["image"]["sha256"],
            "shared original image SHA-256")
    require(len(image) == helper["image"]["size"] == producer["image"]["size"],
            "shared original image size")
    base = int(helper["image"]["conditional_address_base"], 16)
    require(base == int(producer["image"]["image_base"], 16), "shared raw address base")

    byte_context = {row["va"]: row for window in helper["windows"]
                    for row in window["instructions"]}
    byte_context.update({row["va"]: row for row in producer["anchors"]})
    decoded_count = 0
    anchor_count = 0
    for start_text in proof["producer_decode_windows"]:
        window = next(row for row in producer["decode_windows"] if row["start"] == start_text)
        start, end = int(start_text, 16), int(window["end_exclusive"], 16)
        actual = decode(image_path, base, start, end)
        expected = [(va, " ".join(instruction.lower().split()))
                    for va, instruction in window["instructions"]]
        require([(row["va"], row["instruction"]) for row in actual] == expected,
                f"producer fresh decode at {start_text}")
        for row in actual:
            require(row["va"] in byte_context, f"saved byte context at {row['va']}")
            anchor = byte_context[row["va"]]
            offset = int(row["va"], 16) - base
            expected_bytes = bytes.fromhex(anchor["bytes"])
            require(image[offset:offset + len(expected_bytes)] == expected_bytes ==
                    bytes.fromhex(row["bytes"]), f"producer anchor at {row['va']}")
            anchor_count += 1
        decoded_count += len(actual)

    seed = proof["additional_seed_window"]
    start, end = int(seed["start"], 16), int(seed["end_exclusive"], 16)
    require(0 <= start - base < end - base <= len(image), "seed window bounds")
    require(start - base == int(seed["raw_offset"], 16), "seed raw offset")
    raw = image[start - base:end - base]
    require(raw == bytes.fromhex(seed["bytes"]) and sha256(raw) == seed["sha256"],
            "seed window bytes/SHA-256")
    require(bytes.fromhex("".join(row["bytes"] for row in seed["instructions"])) == raw,
            "seed instruction byte coverage")
    require(decode(image_path, base, start, end) == seed["instructions"],
            "seed fresh decode")
    decoded_count += len(seed["instructions"])
    for constant in producer["marker_constants"]:
        offset = int(constant["address"], 16) - base
        require(image[offset:offset + 4] == bytes.fromhex(constant["bytes"]),
                f"producer constant at {constant['address']}")

    print(f"PASS marker composition: {len(proof['pinned_repository_evidence'])} evidence pins; "
          f"{anchor_count} producer anchors; {decoded_count} fresh decoded instructions; "
          f"{len(producer['marker_constants'])} producer constants", flush=True)
    print("BOUNDARY: supplied independent snapshots; transactional no-change is offline API "
          "convention, not original rollback; runtime reachability/owners remain unresolved")


if __name__ == "__main__":
    main()
