#!/usr/bin/env python3
"""Verify frozen bytes and fresh i386 decodes; never execute the source image."""

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


def decode(path, base, start, end):
    result = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={base:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(path)],
        check=True, capture_output=True, text=True, timeout=30)
    rows = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            rows.append([hex(int(match[1], 16)), "".join(match[2].split()),
                         " ".join(match[3].split()).lower()])
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    evidence = json.loads(Path(__file__).with_name("byte_anchors.json").read_text())
    base = int(evidence["base"], 16)
    source = evidence["source"]
    path = args.handoff / source["path"]
    image = path.read_bytes()
    require(hashlib.sha256(image).hexdigest() == source["sha256"], "source hash")
    for reference in evidence["archived_contracts"]:
        data = (args.handoff / reference["path"]).read_bytes()
        require(hashlib.sha256(data).hexdigest() == reference["sha256"],
                f"reference hash: {reference['path']}")
    count = 0
    for window in evidence["windows"]:
        start, end = (int(window[key], 16) for key in ("start", "end"))
        rows = decode(path, base, start, end)
        require(rows == window["instructions"], f"fresh decode: {window['name']}")
        position = start
        for address, encoded, _ in rows:
            raw = bytes.fromhex(encoded)
            require(int(address, 16) == position, f"instruction gap at {address}")
            require(image[position - base:position - base + len(raw)] == raw,
                    f"source bytes at {address}")
            position += len(raw)
            count += 1
        require(position == end, f"window end: {window['name']}")
    for constant in evidence["constants"]:
        offset = int(constant["va"], 16) - base
        raw = bytes.fromhex(constant["bytes"])
        require(image[offset:offset + len(raw)] == raw, f"constant: {constant['name']}")
    require(struct.unpack_from("<d", image, 0x4207A040 - base)[0] == 0.03125,
            "coordinate fraction scale")
    require(struct.unpack_from("<4I", image, 0x4205A560 - base) == (0x80000000,) * 4,
            "coordinate sign xor")
    require(struct.unpack_from("<32I", image, 0x42262664 - base) ==
            tuple(1 << i for i in range(32)), "mask initializer powers")
    print(f"PASS: {len(evidence['windows'])} fresh windows, {count} instruction anchors, "
          f"{len(evidence['constants'])} constants; conditional parser, not a packet/sender")


if __name__ == "__main__":
    main()
