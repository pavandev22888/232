#!/usr/bin/env python3
"""Verify one snapshot-backed dispatch dependency without executing input code."""

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
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    proof = json.loads((here / "dispatch.json").read_text())
    roots = {"repo": repo, "handoff": args.handoff.resolve()}
    image_path = roots["handoff"] / proof["image"]["path"]
    image = image_path.read_bytes()
    require(len(image) == proof["image"]["size"], "whole image size")
    require(digest(image) == proof["image"]["sha256"], "whole image SHA256")
    for source in proof["sources"]:
        require(digest((roots[source["root"]] / source["path"]).read_bytes())
                == source["sha256"], f"source hash: {source['path']}")

    base = int(proof["raw_address_bias"], 16)

    def read(address, size):
        offset = address - base
        require(0 <= offset <= len(image) - size, f"raw range {address:#x}")
        return image[offset:offset + size]

    require(image[:64] == bytes(64), "zero first 64 bytes; no MZ header at offset 0")
    decoded_count = 0
    for window in proof["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(end > start, "nonempty window")
        raw = read(start, end - start)
        require(start - base == int(window["raw_offset"], 16), "raw offset")
        require(raw.hex() == window["bytes"], f"bytes: {window['name']}")
        require(digest(raw) == window["sha256"], f"hash: {window['name']}")
        if "instructions" in window:
            rows = decode(image_path, base, start, end)
            require(rows == window["instructions"], f"fresh decode: {window['name']}")
            cursor = start
            for address, encoded, instruction in rows:
                require(int(address, 16) == cursor, "contiguous instruction boundaries")
                cursor += len(bytes.fromhex(encoded))
                print(f"{address}: {encoded}  {instruction}")
            require(cursor == end, "complete decode window")
            decoded_count += len(rows)
        print(f"WINDOW {window['name']} raw={window['raw_offset']} SHA256={digest(raw)}")

    dependency = proof["dependency"]
    call = int(dependency["call_address"], 16)
    slot = int(dependency["slot_address"], 16)
    thunk = int(dependency["snapshot_u32"], 16)
    require(read(call, 6) == b"\xff\x15" + struct.pack("<I", slot), "CALL absolute slot")
    require(struct.unpack("<I", read(slot, 4))[0] == thunk, "snapshot DWORD")
    require(read(thunk, 1) == b"\xe9", "near JMP opcode")
    displacement = struct.unpack("<i", read(thunk + 1, 4))[0]
    destination = (thunk + 5 + displacement) & 0xffffffff
    require(destination == int(dependency["jmp_destination"], 16), "JMP rel32 arithmetic")
    require(displacement == int(dependency["signed_rel32"], 16), "signed displacement")
    require(read(call - 5, 5) == b"\x68\x40\x49\x00\x00", "literal push 0x4940")
    require(read(call + 6, 3) == b"\x83\xc4\x08", "caller stack adjustment")
    print(f"PASS: {len(proof['sources'])} source hashes, {len(proof['windows'])} windows, "
          f"{decoded_count} fresh i386 instructions; snapshot {slot:#x} -> {thunk:#x} "
          f"-> JMP destination {destination:#x} under raw bias {base:#x}.")
    print("FRONTIER: no runtime binding, import identity, writer/order/lifetime, "
          "callee semantics or allocation/type claim; R03/GAP04 remain open.")


if __name__ == "__main__":
    main()
