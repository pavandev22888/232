#!/usr/bin/env python3
"""Recheck one conditional +0x24 initializer chain without running the image."""

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


def digest(data):
    return hashlib.sha256(data).hexdigest()


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
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    args = parser.parse_args()
    proof = json.loads((here / "initializer.json").read_text())
    roots = {"repo": repo, "handoff": args.handoff.resolve()}
    path = roots["handoff"] / proof["image"]["path"]
    image = path.read_bytes()
    base = int(proof["conditional_base"], 16)
    require(len(image) == proof["image"]["size"], "image size")
    require(digest(image) == proof["image"]["sha256"], "image SHA256")
    for source in proof["sources"]:
        require(digest((roots[source["root"]] / source["path"]).read_bytes())
                == source["sha256"], f"source SHA256: {source['path']}")

    accepted = json.loads((repo / proof["accepted_manifest"]).read_text())
    require(proof["image"] == accepted["image"], "accepted image identity")
    for name in ("selected_install", "fallback_entry"):
        inherited = next(w for w in accepted["windows"] if w["name"] == name)
        current = next(w for w in proof["windows"] if w["name"] == name)
        require(current == inherited, f"unchanged accepted anchor: {name}")

    anchors = {}
    for window in proof["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(0 <= start - base < end - base <= len(image), "window bounds")
        require(start - base == int(window["raw_offset"], 16), "raw offset")
        raw = image[start - base:end - base]
        require(raw.hex() == window["bytes"] and digest(raw) == window["sha256"],
                f"window bytes/SHA256: {window['name']}")
        rows = window["instructions"]
        require(bytes.fromhex("".join(row[1] for row in rows)) == raw,
                f"instruction coverage: {window['name']}")
        require(decode(path, base, start, end) == rows,
                f"fresh decode: {window['name']}")
        anchors.update({int(row[0], 16): row[1:] for row in rows})

    expected = {
        0x41C5E3EB: ["89c6", "mov esi,eax"],
        0x41C5E3ED: ["89c1", "mov ecx,eax"],
        0x41C5BDB7: ["89cf", "mov edi,ecx"],
        0x41C5BDEC: ["8d4f08", "lea ecx,[edi+0x8]"],
        0x41C610AA: ["89ce", "mov esi,ecx"],
        0x41C610CA: ["c7461c8988883c", "mov dword ptr [esi+0x1c],0x3c888889"],
        0x41C5E404: ["89354c195242", "mov dword ptr ds:0x4252194c,esi"],
        0x41C784D8: ["8b0d4c195242", "mov ecx,dword ptr ds:0x4252194c"],
        0x41C784DE: ["f30f105124", "movss xmm2,dword ptr [ecx+0x24]"],
    }
    for address, expected_row in expected.items():
        require(anchors.get(address) == expected_row, f"identity anchor {address:#x}")
    for call, target in ((0x41C5E3F0, 0x41C5BDA0),
                         (0x41C5BDF5, 0x41C610A0),
                         (0x41C610B4, 0x4200CF20)):
        require(image[call - base] == 0xE8, f"rel32 opcode {call:#x}")
        actual = call + 5 + struct.unpack_from("<i", image, call - base + 1)[0]
        require(actual == target, f"rel32 target {call:#x}")

    candidate = proof["candidate"]
    require(int(candidate["parent_subobject_offset"], 16)
            + int(candidate["store_offset"], 16) == 0x24, "same-base offset composition")
    bits = int(candidate["immediate_bits"], 16)
    require(bits == 0x3C888889, "selected immediate")
    value = struct.unpack("<f", struct.pack("<I", bits))[0]
    require(value == candidate["binary32_value"], "immediate binary32 interpretation")
    print(f"PASS W5-FALLBACK24: image; {len(proof['sources'])} pinned sources; "
          f"{len(proof['windows'])} hashed windows; {len(anchors)} fresh instructions; "
          "3 rel32 targets; one candidate immediate/offset composition")
    print(f"STATIC IMMEDIATE: {bits:#010x} -> binary32 {value!r}; not a runtime observation")
    print("CONDITIONAL: nested-call ESI identity, enclosing-call ESI identity, "
          "field stability, publication, lifetime and per-read ordering are NOT proved.")
    print("FRONTIER: R03/GAP04 remain open; no image execution/emulation or new literal inventory.")


if __name__ == "__main__":
    main()
