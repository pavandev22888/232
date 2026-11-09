#!/usr/bin/env python3
"""Recheck one nested-call ESI prerequisite using read-only source bytes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile


def require(value, message):
    if not value:
        raise ValueError(message)


def digest_stream(stream):
    result = hashlib.sha256()
    while block := stream.read(1024 * 1024):
        result.update(block)
    return result.hexdigest()


def source_path(root, relative):
    path = (root / relative).resolve(strict=True)
    require(path.is_relative_to(root), "Source outside declared root")
    return path


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


def closed_leaf(window, proof):
    rows = window["instructions"]
    instructions = {int(va, 16): (data, text) for va, data, text in rows}
    start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
    # None of these instructions has an implicit ESI destination.
    allowed = {"mov", "movzx", "test", "imul", "cmp", "bt", "movd", "pshufd",
               "add", "movups", "and", "sub", "lea", "nop", "movdqa", "movdqu",
               "inc", "ret", "je", "jbe", "jb", "jae", "jne", "jmp", "rep"}
    successors, returns, direct = {}, [], []
    for va, (data, text) in instructions.items():
        mnemonic = text.split()[0]
        require(mnemonic in allowed, f"Unexamined instruction at {va:#x}")
        require(not re.search(r"\b(?:esi|si)\b", text), f"ESI operand at {va:#x}")
        if mnemonic == "rep":
            require(text == "rep stos byte ptr es:[edi],al" and data == "f3aa",
                    "Unexpected implicit-register instruction")
        following = va + len(data) // 2
        if mnemonic == "ret":
            require(data == "c3" and text == "ret", "Unexpected return discipline")
            returns.append(hex(va))
            successors[va] = []
        elif mnemonic.startswith("j"):
            target = int(text.split()[-1], 16)
            direct.append([hex(va), hex(target)])
            successors[va] = [target] if mnemonic == "jmp" else [target, following]
        else:
            successors[va] = [following]
        require(all(target in instructions for target in successors[va]),
                f"Control edge leaves pinned instruction boundaries at {va:#x}")
    require(returns == proof["new_normal_returns"], "Normal return set")
    require(direct == proof["new_direct_edges"], "Direct edge set")
    reached, pending = set(), [start]
    while pending:
        va = pending.pop()
        if va not in reached:
            reached.add(va)
            pending.extend(successors[va])
    require(reached == set(instructions), "Unreached body bytes in local static CFG")
    require(end - start == 346 and len(rows) == 89 and len(direct) == 17,
            "Selected bounded leaf changed")
    return len(reached)


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    args = parser.parse_args()
    roots = {"repo": repo, "handoff": args.handoff.resolve(strict=True)}
    proof = json.loads((here / "anchors.json").read_text())
    for pin in [proof["image"], *proof["sources"]]:
        path = source_path(roots[pin["root"]], pin["path"])
        require(path.stat().st_size == pin["size"], "Source size: " + pin["path"])
        with path.open("rb") as stream:
            require(digest_stream(stream) == pin["sha256"], "Source hash: " + pin["path"])
    for member in proof["archive_members"]:
        archive = source_path(roots["handoff"], member["archive"])
        with zipfile.ZipFile(archive) as source:
            require(source.getinfo(member["member"]).file_size == member["size"],
                    "ZIP member size")
            with source.open(member["member"]) as stream:
                require(digest_stream(stream) == member["sha256"], "ZIP member hash")
    require(proof["archive_members"][0]["sha256"] == proof["image"]["sha256"],
            "Source ZIP member does not match raw image")
    image_path = source_path(roots["handoff"], proof["image"]["path"])
    raw = image_path.read_bytes()
    base = int(proof["image"]["conditional_raw_origin"], 16)
    windows, anchors = {}, {}
    for window in proof["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(0 <= start - base < end - base <= len(raw), "Window bounds")
        data = raw[start - base:end - base]
        require(start - base == int(window["raw_offset"], 16), "Raw offset")
        require(len(data) == window["size"] and data.hex() == window["bytes"]
                and hashlib.sha256(data).hexdigest() == window["sha256"], "Window pin")
        rows = decode(image_path, base, start, end)
        require(rows == window["instructions"], "Fresh decode: " + window["name"])
        cursor = start
        for address, encoded, text in rows:
            va = int(address, 16)
            require(va == cursor and va not in anchors, "Instruction coverage")
            cursor += len(encoded) // 2
            anchors[va] = [encoded, text]
        require(cursor == end, "Incomplete decode")
        require(bytes.fromhex("".join(row[1] for row in rows)) == data, "Decode bytes")
        windows[window["name"]] = window
    inherited = json.loads((repo / "continuation/evidence/wave5/fallback24/initializer.json").read_text())
    for name in ("constructor_route", "initializer_candidate"):
        prior = next(w for w in inherited["windows"] if w["name"] == name)
        require(all(windows[name][key] == prior[key] for key in
                    ("start", "end_exclusive", "raw_offset", "sha256", "bytes", "instructions")),
                "Inherited W5 window changed")
    for call, target in ((0x41C5BDF5, 0x41C610A0), (0x41C610B4, 0x4200CF20)):
        offset = call - base
        require(raw[offset] == 0xE8 and call + 5 + struct.unpack_from("<i", raw, offset + 1)[0]
                == target, "Direct call target")
    for va, expected in {
        0x41C5BDB7: ["89cf", "mov edi,ecx"],
        0x41C5BDEC: ["8d4f08", "lea ecx,[edi+0x8]"],
        0x41C610AA: ["89ce", "mov esi,ecx"],
        0x41C610AC: ["6820150000", "push 0x1520"],
        0x41C610B1: ["6a00", "push 0x0"],
        0x41C610B3: ["51", "push ecx"],
        0x41C610B9: ["83c40c", "add esp,0xc"],
        0x41C610CA: ["c7461c8988883c", "mov dword ptr [esi+0x1c],0x3c888889"],
    }.items():
        require(anchors.get(va) == expected, f"Identity anchor {va:#x}")
    require(8 + 0x1C == 0x24, "Parent field composition")
    count = closed_leaf(windows["nested_leaf"], proof)
    print(json.dumps({
        "passed": True, "task": "W8-PROVIDER", "source_pins": 1 + len(proof["sources"]),
        "zip_members": len(proof["archive_members"]), "windows": len(windows),
        "fresh_instructions": len(anchors), "new_leaf_bytes": 346,
        "new_leaf_instructions": count, "new_leaf_direct_edges": 17,
        "normal_returns": proof["new_normal_returns"],
        "resolved": "S_store=S_entry across the nominated 0x4200CF20 call on supported normal flow",
        "frontier": "Later field stability, actual pointer publication/read identity, lifetime, runtime mapping and language/standard remain unresolved",
        "original_executed_loaded_emulated": False,
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
