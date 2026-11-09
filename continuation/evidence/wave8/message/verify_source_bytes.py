#!/usr/bin/env python3
"""Verify the receiver suffix against pinned source/ZIP bytes without execution."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


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


def verify_file(root, description):
    path = root / description["path"]
    data = path.read_bytes()
    if "size_bytes" in description:
        require(len(data) == description["size_bytes"], f"size: {description['path']}")
    require(hashlib.sha256(data).hexdigest() == description["sha256"],
            f"SHA256: {description['path']}")
    return path, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    evidence = json.loads(Path(__file__).with_name("byte_anchors.json").read_text())
    repo = Path(__file__).resolve().parents[4]
    for reference in evidence["preceding_evidence"]:
        verify_file(repo, reference)
    source = evidence["source"]
    path, image = verify_file(args.handoff, source)
    archive, _ = verify_file(args.handoff, evidence["source_archive"])
    with zipfile.ZipFile(archive) as zipped:
        description = evidence["source_archive"]
        member = zipped.read(description["member"])
        require(len(member) == description["member_size_bytes"], "archive member size")
        require(hashlib.sha256(member).hexdigest() == description["member_sha256"],
                "archive member SHA256")
        require(member == image, "archive/restored source identity")
    print(f"PASS source/ZIP/member SHA256 and identity: {source['sha256']}")

    base = int(source["image_base"], 16)
    total = 0
    for window in evidence["windows"]:
        start, end = (int(window[key], 16) for key in ("start", "end"))
        require(0 <= start - base < end - base <= len(image),
                f"bounds: {window['name']}")
        rows = decode(path, base, start, end)
        require(rows == window["instructions"], f"fresh decode: {window['name']}")
        cursor = start
        for address, raw, _ in rows:
            encoded = bytes.fromhex(raw)
            require(int(address, 16) == cursor, f"instruction gap at {address}")
            require(image[cursor - base:cursor - base + len(encoded)] == encoded,
                    f"source bytes at {address}")
            cursor += len(encoded)
        require(cursor == end, f"window end: {window['name']}")
        total += len(rows)
        print(f"PASS {window['name']}: {len(rows)} fresh contiguous byte/decode pins")
    print(f"PASS {total} instructions; conditional receiver prefix only")
    print("FRONTIER 0x4150afb4: next TLS epoch gate; side exits "
          "0x4150b77c / 0x4150b831 / 0x4150ac88 remain unimplemented")


if __name__ == "__main__":
    main()
