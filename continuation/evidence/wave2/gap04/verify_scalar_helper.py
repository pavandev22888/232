#!/usr/bin/env python3
"""Read-only byte/hash/fresh-decode check; never loads or executes an image."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=repo / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    handoff = args.handoff.resolve()
    proof = json.loads((here / "scalar_helper.json").read_text())
    image_path = handoff / proof["image"]["path"]
    image = image_path.read_bytes()
    require(len(image) == proof["image"]["size"], "image size")
    require(sha256(image) == proof["image"]["sha256"], "image SHA-256")
    base = int(proof["image"]["conditional_address_base"], 16)

    for source in proof["reference_sources"]:
        require(sha256((handoff / source["path"]).read_bytes()) == source["sha256"],
                f"reference source hash: {source['path']}")

    decoded_count = 0
    for window in proof["windows"]:
        start = int(window["start"], 16)
        end = int(window["end_exclusive"], 16)
        require(0 <= start - base < end - base <= len(image), "window bounds")
        require(start - base == int(window["raw_offset"], 16), "raw offset")
        raw = image[start - base:end - base]
        require(raw == bytes.fromhex(window["bytes"]), f"{window['name']}: bytes")
        require(sha256(raw) == window["sha256"], f"{window['name']}: hash")
        expected = window["instructions"]
        require(bytes.fromhex("".join(row["bytes"] for row in expected)) == raw,
                f"{window['name']}: instruction byte coverage")
        result = subprocess.run(
            ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
             "--insn-width=16", f"--adjust-vma={base:#x}",
             f"--start-address={start:#x}", f"--stop-address={end:#x}", str(image_path)],
            capture_output=True, text=True, check=True, timeout=30)
        actual = []
        for line in result.stdout.splitlines():
            match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
            if match:
                actual.append({"va": hex(int(match[1], 16)),
                               "bytes": "".join(match[2].split()),
                               "instruction": " ".join(match[3].split()).lower()})
        require(actual == expected, f"{window['name']}: fresh decode differs")
        decoded_count += len(actual)

    for constant in proof["constants"]:
        start = int(constant["va"], 16) - base
        expected = bytes.fromhex(constant["bytes"])
        raw = image[start:start + len(expected)]
        require(raw == expected and sha256(raw) == constant["sha256"],
                f"constant {constant['name']}")

    for call in proof["direct_calls"]:
        va = int(call["va"], 16)
        offset = va - base
        require(image[offset] == 0xE8, f"direct call opcode at {va:#x}")
        target = va + 5 + struct.unpack_from("<i", image, offset + 1)[0]
        require(target == int(call["target"], 16), f"direct call target at {va:#x}")

    print(f"PASS scalar helper: image hash; {len(proof['reference_sources'])} reference hashes; "
          f"{decoded_count} instruction bytes + fresh decodes; "
          f"{len(proof['constants'])} constants; {len(proof['direct_calls'])} direct calls")
    print("UNRESOLVED: fallback-step contents/owner, actor tolerance, upstream guards, "
          "runtime mapping/reachability, and unrelated 0x289A6D60 binding")


if __name__ == "__main__":
    main()
