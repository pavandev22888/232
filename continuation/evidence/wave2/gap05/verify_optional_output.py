#!/usr/bin/env python3
"""Verify static GAP05 wave2 bytes and fresh decode; never load the source image."""

import argparse
import hashlib
import json
import re
import struct
import subprocess
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def main():
    script = Path(__file__).resolve()
    repo = script.parents[4]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=repo / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    handoff = args.handoff.resolve()
    output = (repo / "runs/wave2/gap05").resolve()
    require(output != handoff and handoff not in output.parents,
            "temporary output directory must be outside the read-only handoff")

    evidence = json.loads((script.parent / "optional_output.json").read_text())
    image_spec = evidence["image"]
    image = (handoff / image_spec["handoff_relative_path"]).read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    require(digest == image_spec["sha256"], "source image SHA-256 mismatch")
    require(len(image) == image_spec["size"], "source image size mismatch")
    base = int(image_spec["base"], 16)

    version = subprocess.run(["objdump", "--version"], check=True,
                             capture_output=True, text=True).stdout.splitlines()[0]
    output.mkdir(parents=True, exist_ok=True)
    instruction_count = 0
    byte_count = 0
    for window in evidence["decode_windows"]:
        start = int(window["start"], 16)
        end = int(window["end_exclusive"], 16)
        require(base <= start < end <= base + len(image),
                f"window outside image: {window['name']}")
        raw = image[start - base:end - base]
        require(raw.hex() == window["bytes"], f"window bytes: {window['name']}")
        require(hashlib.sha256(raw).hexdigest() == window["sha256"],
                f"window SHA-256: {window['name']}")
        with tempfile.TemporaryDirectory(prefix="decode-", dir=output) as temp:
            snippet = Path(temp) / "window.bin"
            snippet.write_bytes(raw)
            result = subprocess.run(
                ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                 "--insn-width=16", f"--adjust-vma={start}", str(snippet)],
                check=True, capture_output=True, text=True)
        decoded = []
        for line in result.stdout.splitlines():
            match = re.fullmatch(r"\s*([0-9a-f]+):\s*\t([0-9a-f ]+)\t(.+)", line)
            if match:
                address, hex_bytes, instruction = match.groups()
                decoded.append({
                    "va": f"0x{int(address, 16):08x}",
                    "bytes": "".join(hex_bytes.split()),
                    "instruction": " ".join(instruction.lower().split())
                })
        require(decoded == window["instructions"],
                f"fresh instruction decode: {window['name']}")
        cursor = start
        for instruction in decoded:
            require(int(instruction["va"], 16) == cursor,
                    f"instruction coverage hole at {cursor:#x}")
            cursor += len(bytes.fromhex(instruction["bytes"]))
        require(cursor == end, f"incomplete window decode: {window['name']}")
        instruction_count += len(decoded)
        byte_count += len(raw)

    for constant in evidence["constants"]:
        offset = int(constant["va"], 16) - base
        require(0 <= offset <= len(image) - 4, "constant outside image")
        raw = image[offset:offset + 4]
        require(raw.hex() == constant["bytes"], f"constant bytes: {constant['va']}")
        require(struct.unpack("<f", raw)[0] == constant["float"],
                f"constant float: {constant['va']}")

    print(f"PASS: image SHA-256={digest}; windows={len(evidence['decode_windows'])}; "
          f"bytes={byte_count}; fresh decoded instructions={instruction_count}; "
          f"constants={len(evidence['constants'])}; handoff read-only")
    print(version)


if __name__ == "__main__":
    main()
