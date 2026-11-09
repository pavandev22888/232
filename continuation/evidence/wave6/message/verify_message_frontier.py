#!/usr/bin/env python3
"""Verify pinned message-caller bytes and fresh i386 decodes without execution."""

import argparse
import hashlib
import json
from pathlib import Path
import re
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
    rows = {}
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            address = hex(int(match[1], 16))
            rows[address] = ["".join(match[2].split()),
                             " ".join(match[3].split()).lower()]
    return rows


def rotate(value, count, left):
    count &= 31
    value &= 0xffffffff
    if count == 0:
        return value
    if left:
        return ((value << count) | (value >> (32 - count))) & 0xffffffff
    return ((value >> count) | (value << (32 - count))) & 0xffffffff


def derive_dispatch_address(frontier):
    value = int(frontier["static_derivation"]["initial_eax"], 16)
    for operation in frontier["static_derivation"]["operations"]:
        if operation[0] == "ror":
            value = rotate(value, operation[1], False)
        elif operation[0] == "rol":
            value = rotate(value, operation[1], True)
        elif operation[0] == "xor":
            value ^= int(operation[1], 16)
        else:
            raise SystemExit(f"FAIL: unsupported derivation operation {operation[0]}")
        value &= 0xffffffff
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    evidence_path = Path(__file__).with_name("byte_anchors.json")
    evidence = json.loads(evidence_path.read_text())
    source = evidence["source"]
    path = args.handoff / source["path"]
    image = path.read_bytes()
    require(len(image) == source["size_bytes"], "source size")
    require(hashlib.sha256(image).hexdigest() == source["sha256"], "source SHA256")

    repo = Path(__file__).resolve().parents[4]
    prior = repo / evidence["preceding_evidence"]["path"]
    require(hashlib.sha256(prior.read_bytes()).hexdigest() ==
            evidence["preceding_evidence"]["sha256"], "W5 evidence SHA256")

    base = int(source["image_base"], 16)
    for window in evidence["windows"]:
        start, end = (int(window[key], 16) for key in ("start", "end"))
        require(0 <= start - base < end - base <= len(image),
                f"window bounds: {window['name']}")
        rows = decode(path, base, start, end)
        for address, raw, assembly in window["instructions"]:
            actual = rows.get(address)
            require(actual == [raw, assembly],
                    f"decode mismatch at {address}: {actual!r}")
            offset = int(address, 16) - base
            require(image[offset:offset + len(bytes.fromhex(raw))].hex() == raw,
                    f"bytes mismatch at {address}")
        print(f"PASS {window['name']}: {len(rows)} fresh decoded instructions; "
              f"{len(window['instructions'])} byte/decode pins")

    frontier = evidence["frontier"]
    address = derive_dispatch_address(frontier)
    require(address == int(frontier["computed_dispatch_object"], 16),
            "computed dispatch object")
    offset = address - base
    require(not 0 <= offset < len(image),
            "computed dispatch object unexpectedly resolves inside nl.bin")
    print(f"PASS source SHA256 {source['sha256']}")
    print(f"PASS dispatch expression -> {address:#010x}, outside pinned image")
    print(f"FRONTIER {frontier['address']}: FS-relative state; guard input {frontier['guard_input']}; "
          f"unequal register jump and equal-edge runtime dereference remain unresolved")


if __name__ == "__main__":
    main()
