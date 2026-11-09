#!/usr/bin/env python3
"""Verify bounded W3-S provenance bytes; handoff and image are read-only data."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def read_pinned(handoff, specification):
    path = (handoff / specification["path"]).resolve()
    require(handoff in path.parents, "input path must stay inside handoff")
    raw = path.read_bytes()
    require(len(raw) == specification["size"], f"input size: {specification['path']}")
    require(hashlib.sha256(raw).hexdigest() == specification["sha256"],
            f"input SHA-256: {specification['path']}")
    return path, raw


def archived_instructions(value):
    if isinstance(value, dict):
        if all(key in value for key in ("va", "bytes", "instruction")):
            yield (int(value["va"], 16), value["bytes"].lower(),
                   " ".join(value["instruction"].lower().split()))
        for child in value.values():
            yield from archived_instructions(child)
    elif isinstance(value, list):
        for child in value:
            yield from archived_instructions(child)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    handoff = args.handoff.resolve()
    require(handoff.is_dir(), "handoff directory does not exist")
    ledger = json.loads((Path(__file__).resolve().parent / "ledger.json").read_text())
    image_path, image = read_pinned(handoff, ledger["image"])
    base = int(ledger["image"]["base"], 16)
    archive = set()
    for specification in ledger["source_inputs"]:
        _, raw = read_pinned(handoff, specification)
        archive.update(archived_instructions(json.loads(raw)))

    version = subprocess.run(["objdump", "--version"], check=True,
                             capture_output=True, text=True).stdout.splitlines()[0]
    require(version.startswith("GNU objdump"), "GNU objdump required")
    instruction_count = 0
    byte_count = 0
    anchored = set()
    for window in ledger["decode_windows"]:
        start = int(window["start"], 16)
        end = int(window["end_exclusive"], 16)
        require(base <= start < end <= base + len(image), "window outside image")
        raw = image[start - base:end - base]
        require(len(raw) == window["size"], f"window size: {window['name']}")
        require(hashlib.sha256(raw).hexdigest() == window["sha256"],
                f"window SHA-256: {window['name']}")
        result = subprocess.run(
            ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
             "--insn-width=16", f"--adjust-vma={base}",
             f"--start-address={start}", f"--stop-address={end}", str(image_path)],
            check=True, capture_output=True, text=True)
        decoded = []
        cursor = start
        for line in result.stdout.splitlines():
            match = re.fullmatch(r"\s*([0-9a-f]+):\s*\t([0-9a-f ]+)\t(.+)", line)
            if not match:
                continue
            address, hex_bytes, instruction = match.groups()
            address = int(address, 16)
            hex_bytes = "".join(hex_bytes.split())
            instruction = " ".join(instruction.lower().split())
            require(address == cursor, f"decode coverage at {cursor:#x}")
            size = len(bytes.fromhex(hex_bytes))
            require(size > 0 and address + size <= end, "partial boundary instruction")
            require(image[address - base:address - base + size].hex() == hex_bytes,
                    f"instruction bytes at {address:#x}")
            require((address, hex_bytes, instruction) in archive,
                    f"instruction absent from pinned archival context at {address:#x}")
            decoded.append({"va": f"0x{address:08x}", "bytes": hex_bytes,
                            "instruction": instruction})
            anchored.add(f"0x{address:08x}")
            cursor += size
        require(cursor == end, f"incomplete decode: {window['name']}")
        require(decoded == window["instructions"], f"fresh decode: {window['name']}")
        instruction_count += len(decoded)
        byte_count += len(raw)

    for claim in ledger["claims"]:
        require(set(claim["anchors"]) <= anchored, f"unverified anchors: {claim['id']}")
    layout = ledger["layout"]
    stride = int(layout["stride"], 16)
    require(int(layout["allocation_size"], 16) == stride * layout["capacity"],
            "record capacity arithmetic")
    require(int(layout["pair_stride"], 16) == 2 * stride, "paired record stride")
    print(f"PASS: image SHA-256={ledger['image']['sha256']}; "
          f"pinned sources={len(ledger['source_inputs'])}; "
          f"windows={len(ledger['decode_windows'])}; bytes={byte_count}; "
          f"fresh decoded instructions={instruction_count}; "
          f"claims with byte anchors={len(ledger['claims'])}")
    print(version)
    print("Read-only: no output files, imports from handoff, original loading, execution or emulation.")
    print("Open: upstream P/P+0 source, cross-call instance identity, other writers/aliases, reachability.")


if __name__ == "__main__":
    main()
