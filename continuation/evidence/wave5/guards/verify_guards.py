#!/usr/bin/env python3
"""Read-only W5 guard/index boundary byte and direct-edge verification."""

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


def decode(image, base, start, end):
    result = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={base}",
         f"--start-address={start}", f"--stop-address={end}", str(image)],
        check=True, capture_output=True, text=True, timeout=30)
    rows = []
    for line in result.stdout.splitlines():
        match = re.fullmatch(r"\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*", line)
        if match:
            rows.append([hex(int(match[1], 16)), "".join(match[2].split()),
                         " ".join(match[3].split())])
    return rows


def verify(handoff):
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    proof = json.loads((here / "guard_path.json").read_text())
    roots = {"repo": repo, "handoff": handoff.resolve()}
    sources = {}
    for source in proof["sources"]:
        path = roots[source["root"]] / source["path"]
        raw = path.read_bytes()
        require(digest(raw) == source["sha256"], f"source SHA256: {source['path']}")
        if source["path"].endswith("_walk.json"):
            sources[source["id"]] = {int(row["va"], 16): row
                                      for row in json.loads(raw)["instructions"]}

    image_path = handoff.resolve() / proof["image"]["path"]
    image = image_path.read_bytes()
    require(len(image) == proof["image"]["size"], "whole image size")
    require(digest(image) == proof["image"]["sha256"], "whole image SHA256")
    base = int(proof["image"]["conditional_base"], 16)
    instructions = {}
    for window in proof["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(0 <= start - base < end - base <= len(image), "raw image bounds")
        require(start - base == int(window["raw_offset"], 16), "raw offset")
        raw = image[start - base:end - base]
        require(raw.hex() == window["bytes"] and digest(raw) == window["sha256"],
                f"window bytes/hash: {window['id']}")
        rows = window["instructions"]
        require(decode(image_path, base, start, end) == rows,
                f"fresh i386 decode: {window['id']}")
        cursor = start
        for address, encoded, instruction in rows:
            address = int(address, 16)
            require(address == cursor, "contiguous instruction coverage")
            original = sources[window["source"]][address]
            require(original["bytes"] == encoded and
                    " ".join(original["instruction"].split()) == instruction,
                    f"archived walk cross-check: {hex(address)}")
            require(original["size"] == len(bytes.fromhex(encoded)), "instruction size")
            instructions[hex(address)] = (bytes.fromhex(encoded), instruction)
            cursor += len(bytes.fromhex(encoded))
        require(cursor == end, "window end")

    for edge in proof["direct_edges"]:
        encoded, instruction = instructions[edge["at"]]
        opcode = bytes.fromhex(edge["opcode"])
        require(encoded.startswith(opcode), f"edge opcode: {edge['at']}")
        displacement = encoded[len(opcode):]
        require(len(displacement) in (1, 4), "relative displacement width")
        value = struct.unpack("<b" if len(displacement) == 1 else "<i", displacement)[0]
        next_address = int(edge["at"], 16) + len(encoded)
        require(next_address + value == int(edge["target"], 16), "relative target")
        require(instruction == f"{edge['mnemonic']} {edge['target']}", "edge decode")
        if "fallthrough" in edge:
            require(next_address == int(edge["fallthrough"], 16), "fallthrough address")

    for boundary in proof["boundaries"]:
        require(boundary["status"] == "unresolved", "boundary cannot imply closure")
        require(all(address in instructions for address in boundary["anchors"]),
                f"boundary anchors: {boundary['id']}")

    print(subprocess.check_output(["objdump", "--version"], text=True).splitlines()[0])
    print(f"PASS: {len(proof['sources'])} source hashes, whole image SHA256, "
          f"{len(proof['windows'])} windows, {len(instructions)} instructions, "
          f"{len(proof['direct_edges'])} direct edges, "
          f"{len(proof['boundaries'])} distinct unresolved boundaries")
    print("Static local evidence only; no loading, execution, emulation, TLS resolution, "
          "callee contracts, original fault result or runtime equivalence.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    verify(parser.parse_args().handoff)
