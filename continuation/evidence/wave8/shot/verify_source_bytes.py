#!/usr/bin/env python3
"""Check pinned static shot-index windows; never load or execute the original."""

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


def verify_file(root, description):
    path = root / description["path"]
    data = path.read_bytes()
    require(len(data) == description["size_bytes"], f"size: {description['path']}")
    require(hashlib.sha256(data).hexdigest() == description["sha256"],
            f"SHA256: {description['path']}")
    return path, data


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
    pins = json.loads(Path(__file__).with_name("byte_anchors.json").read_text())
    repo = Path(__file__).resolve().parents[4]
    for reference in pins["navigation_receipts"]:
        root = repo if reference["root"] == "git" else args.handoff
        verify_file(root, reference)
    path, image = verify_file(args.handoff, pins["source"])
    archive, _ = verify_file(args.handoff, pins["source_archive"])
    description = pins["source_archive"]
    with zipfile.ZipFile(archive) as zipped:
        member = zipped.read(description["member"])
    require(len(member) == description["member_size_bytes"], "ZIP member size")
    require(hashlib.sha256(member).hexdigest() == description["member_sha256"],
            "ZIP member SHA256")
    require(member == image, "ZIP/restored source identity")
    print(f"PASS source/ZIP/member hashes and identity: {pins['source']['sha256']}")
    print(f"PASS {len(pins['navigation_receipts'])} navigation receipt hashes")

    base = int(pins["source"]["image_base"], 16)
    total = 0
    for window in pins["windows"]:
        start, end = (int(window[key], 16) for key in ("start", "end"))
        require(0 <= start - base < end - base <= len(image), window["name"])
        rows = decode(path, base, start, end)
        require(rows == window["instructions"], f"fresh decode: {window['name']}")
        cursor = start
        for address, raw, _ in rows:
            encoded = bytes.fromhex(raw)
            require(int(address, 16) == cursor, f"instruction gap: {address}")
            require(image[cursor - base:cursor - base + len(encoded)] == encoded,
                    f"source bytes: {address}")
            cursor += len(encoded)
        require(cursor == end, f"window end: {window['name']}")
        total += len(rows)
        print(f"PASS {window['name']}: {len(rows)} contiguous byte/decode pins")
    print(f"PASS {total} instructions; bounded raw index/store/postcall consumer only")
    print("FRONTIER opaque 0x413F8AB0 / 0x414BD940 and intervening CFG; "
          "successors 0x414C2003 / 0x414C2233; R linkage and result semantics unproved")


if __name__ == "__main__":
    main()
