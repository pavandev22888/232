#!/usr/bin/env python3
"""Verify all eleven raw local reset stores; not entry or call scheduling."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
HELPERS = runpy.run_path(str(HERE.parents[1] / "wave2/gap03/verify_reset_evidence.py"))
require = HELPERS["require"]
normalized = HELPERS["normalized"]
fresh_decode = HELPERS["fresh_decode"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    evidence = json.loads((HERE / "full_reset_evidence.json").read_text())
    workspace = args.handoff.resolve() / "workspace"
    for relative, expected in evidence["source_sha256"].items():
        path = workspace / relative
        require(path.is_file(), f"missing source: {relative}")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected,
                f"source hash mismatch: {relative}")

    image_path = workspace / evidence["image"]["path"]
    image = image_path.read_bytes()
    base = int(evidence["image"]["image_base"], 16)
    block = evidence["block"]
    start, end = int(block["start"], 16), int(block["end_exclusive"], 16)
    walk = json.loads((workspace / block["walk"]).read_text())
    saved = {row["va"]: row for row in walk["instructions"]}
    decoded = fresh_decode(image_path, base, start, end)
    stores = evidence["stores"]
    require(len(stores) == 11 and
            [row["width"] for row in stores].count(1) == 3 and
            [row["width"] for row in stores].count(4) == 8,
            "expected three BYTE and eight DWORD stores")
    cursor = start
    destinations = set()
    for row in stores:
        va = int(row["va"], 16)
        width = row["width"]
        address = int(row["address"], 16)
        require(width in (1, 4) and row["value"] == 0,
                f"invalid zero store: {row['va']}")
        expected = ((b"\xc6\x05" if width == 1 else b"\xc7\x05") +
                    address.to_bytes(4, "little") + bytes(width))
        member = ("byte_" if width == 1 else "dword_") + f"{address:08x}"
        require(row["snapshot_member"] == member,
                f"address-derived snapshot member mismatch: {row['va']}")
        require(va == cursor and row["size"] == len(expected) and
                bytes.fromhex(row["bytes"]) == expected,
                f"store encoding/width/order mismatch: {row['va']}")
        require(saved.get(row["va"]) ==
                {key: row[key] for key in ("va", "bytes", "size", "instruction")},
                f"saved walk mismatch: {row['va']}")
        offset = va - base
        require(offset >= 0 and image[offset:offset + len(expected)] == expected,
                f"original byte mismatch: {row['va']}")
        require(decoded.get(va) == (expected, normalized(row["instruction"])),
                f"fresh objdump mismatch: {row['va']}")
        require(address not in destinations, "duplicate store destination")
        destinations.add(address)
        cursor += len(expected)
    require(cursor == end and set(decoded) == {int(row["va"], 16) for row in stores},
            "incomplete local block")
    print("PASS: 3 immutable source hashes, 11 zero stores/fresh objdump instructions "
          "(3 BYTE, 8 DWORD); entry and scheduling remain unresolved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
