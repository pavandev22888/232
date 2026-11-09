#!/usr/bin/env python3
"""Check pinned seed-path bytes and fresh static decoding; never execute the image."""

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def main():
    directory = Path(__file__).resolve().parent
    repo = directory.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    handoff = args.handoff.resolve()
    output = (repo / "runs/wave5/lifecycle").resolve()
    require(handoff != output and handoff not in output.parents,
            "temporary files must be outside the read-only handoff")
    proof = json.loads((directory / "seed_path.json").read_text())
    spec = proof["image"]
    image_path = handoff / spec["handoff_relative_path"]
    image = image_path.read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    require(digest == spec["sha256"] and len(image) == spec["size"],
            "original image size/SHA-256")
    base = int(spec["base"], 16)
    for source in proof["reference_pins"]:
        root = handoff if source["root"] == "handoff" else repo
        require(hashlib.sha256((root / source["path"]).read_bytes()).hexdigest() ==
                source["sha256"], f"reference pin {source['path']}")

    output.mkdir(parents=True, exist_ok=True)
    count = 0
    byte_count = 0
    for window in proof["decode_windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(base <= start < end <= base + len(image), "window bounds")
        raw = image[start - base:end - base]
        require(raw.hex() == window["bytes"] and
                hashlib.sha256(raw).hexdigest() == window["sha256"],
                f"window bytes/SHA-256 {window['name']}")
        with tempfile.TemporaryDirectory(prefix="decode-", dir=output) as temporary:
            snippet = Path(temporary) / "window.bin"
            snippet.write_bytes(raw)
            result = subprocess.run(
                ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                 "--insn-width=16", f"--adjust-vma={start}", str(snippet)],
                capture_output=True, text=True, check=True)
        decoded = []
        cursor = start
        for line in result.stdout.splitlines():
            match = re.fullmatch(r"\s*([0-9a-f]+):\s*\t([0-9a-f ]+)\t(.+)", line)
            if not match:
                continue
            address, raw_bytes, instruction = match.groups()
            require(int(address, 16) == cursor, "contiguous instruction coverage")
            encoded = "".join(raw_bytes.split())
            cursor += len(bytes.fromhex(encoded))
            decoded.append({"va": f"0x{int(address, 16):08x}", "bytes": encoded,
                            "instruction": " ".join(instruction.lower().split())})
        require(cursor == end and decoded == window["instructions"],
                f"fresh decode {window['name']}")
        count += len(decoded)
        byte_count += len(raw)

    for constant in proof["constants"]:
        offset = int(constant["va"], 16) - base
        require(0 <= offset <= len(image) - 4, "constant bounds")
        require(image[offset:offset + 4].hex() == constant["bytes"],
                f"constant {constant['va']}")
    require(hashlib.sha256(image_path.read_bytes()).hexdigest() == digest,
            "original image unchanged after static decoding")
    print(f"PASS W5-LIFECYCLE: {len(proof['reference_pins'])} reference pins; "
          f"{count} instructions / {byte_count} bytes; {len(proof['constants'])} constants")
    print("BOUNDARY: selected AL!=0 branch only; guards, time/provider owners, other "
          "seed branches and full lifecycle remain unresolved; no original execution")


if __name__ == "__main__":
    main()
