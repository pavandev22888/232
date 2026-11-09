#!/usr/bin/env python3
"""Verify a bounded static ESI save/restore chain; never run the image."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
BASE = 0x412A0000
START, END = 0x41C5BDA0, 0x41C5C719


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def decode(path, start, end):
    result = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={BASE:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(path)],
        check=True, capture_output=True, text=True, timeout=30)
    rows = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            rows.append([hex(int(match[1], 16)), "".join(match[2].split()),
                         " ".join(match[3].split()).lower()])
    return rows


def read_table():
    lines = (HERE / "callee.tsv").read_text().splitlines()
    require(lines[0] == "address\tbytes\tinstruction", "table header")
    rows = [line.split("\t") for line in lines[1:]]
    require(all(len(row) == 3 for row in rows), "table columns")
    return rows


def check_local_structure(rows):
    expected_mnemonics = {
        "push": 17, "mov": 198, "and": 1, "sub": 1, "lea": 34, "call": 9,
        "xorps": 5, "movlps": 4, "movups": 84, "movsd": 8, "add": 6,
        "movaps": 6, "test": 2, "sete": 1, "je": 1, "inc": 1, "pop": 4,
        "ret": 1,
    }
    require(dict(Counter(row[2].split()[0] for row in rows)) == expected_mnemonics,
            "bounded instruction set/counts; implicit effects not generalized")
    cursor = START
    for address, raw, _ in rows:
        require(int(address, 16) == cursor, "continuous entry-rooted boundaries")
        cursor += len(bytes.fromhex(raw))
    require(cursor == END, "callee interval end")
    writes = [row for row in rows if re.match(
        r"(?:mov|lea|and|sub|add|inc|sete|pop) (?:esi|si|ebp|bp)(?:,|$)", row[2])]
    require(writes == [
        ["0x41c5bda1", "89e5", "mov ebp,esp"],
        ["0x41c5bdaf", "89e6", "mov esi,esp"],
        ["0x41c5c712", "5e", "pop esi"],
        ["0x41c5c715", "5d", "pop ebp"],
    ], "explicit ESI/EBP writes")
    transfers = [row for row in rows if re.match(r"(?:j\w+|ret\w*)\b", row[2])]
    require(transfers == [
        ["0x41c5c24e", "7406", "je 0x41c5c256"],
        ["0x41c5c716", "c20400", "ret 0x4"],
    ], "only supported local branch and normal return; calls are opaque")
    by_address = {row[0]: row for row in rows}
    require(by_address["0x41c5c250"] == [
        "0x41c5c250", "ff8080030000", "inc dword ptr [eax+0x380]"],
        "branch fallthrough rejoins taken edge at 0x41c5c256")
    require(rows[:8] == [
        ["0x41c5bda0", "55", "push ebp"],
        ["0x41c5bda1", "89e5", "mov ebp,esp"],
        ["0x41c5bda3", "53", "push ebx"],
        ["0x41c5bda4", "57", "push edi"],
        ["0x41c5bda5", "56", "push esi"],
        ["0x41c5bda6", "83e4f0", "and esp,0xfffffff0"],
        ["0x41c5bda9", "81ecb0000000", "sub esp,0xb0"],
        ["0x41c5bdaf", "89e6", "mov esi,esp"],
    ], "save before stack alignment and ESI mutation")
    require(rows[-6:] == [
        ["0x41c5c70f", "8d65f4", "lea esp,[ebp-0xc]"],
        ["0x41c5c712", "5e", "pop esi"],
        ["0x41c5c713", "5f", "pop edi"],
        ["0x41c5c714", "5b", "pop ebx"],
        ["0x41c5c715", "5d", "pop ebp"],
        ["0x41c5c716", "c20400", "ret 0x4"],
    ], "EBP-relative restore and ret 4")
    stores = []
    for _, _, instruction in rows:
        destination = instruction.split(",")[0]
        if "[esi" in destination and not instruction.startswith("lea "):
            match = re.fullmatch(r"mov dword ptr \[esi(?:\+0x([0-9a-f]+))?\],.+",
                                 instruction)
            require(match is not None, "only bounded direct ESI-frame DWORD stores")
            stores.append(int(match[1] or "0", 16))
    require(len(stores) == 57 and min(stores) == 0 and max(stores) + 4 == 0xAC,
            "local-frame store extent; other aliases remain assumptions")


def verify(handoff):
    proof = json.loads((HERE / "preservation.json").read_text())
    image_path = handoff / proof["image"]["path"]
    image = image_path.read_bytes()
    require(len(image) == proof["image"]["size"] and
            sha256(image) == proof["image"]["sha256"], "whole image size/hash")
    for source in proof["sources"]:
        if "commit" in source:
            data = subprocess.check_output(
                ["git", "show", f"{source['commit']}:{source['path']}"], cwd=REPO)
        else:
            data = (REPO / source["path"]).read_bytes()
        require(sha256(data) == source["sha256"], f"source hash: {source['path']}")
    rows = read_table()
    window = proof["callee"]
    raw = image[START - BASE:END - BASE]
    require(window["start"] == hex(START) and window["end_exclusive"] == hex(END)
            and int(window["raw_offset"], 16) == START - BASE, "callee mapping")
    require(len(rows) == window["instruction_count"] and
            sha256((HERE / "callee.tsv").read_bytes()) == window["table_sha256"],
            "frozen callee decode table")
    require(sha256(raw) == window["sha256"] and
            bytes.fromhex("".join(row[1] for row in rows)) == raw, "callee bytes/hash")
    require(decode(image_path, START, END) == rows, "fresh continuous callee decode")
    check_local_structure(rows)
    require([row for row in rows if row[2].startswith("call ")] == proof["nested_calls"],
            "nine opaque nested-call sites; no target-body analysis")
    for anchor in proof["anchors"]:
        require(anchor in rows, f"anchor {anchor[0]}")
    caller = proof["caller"]
    start, end = int(caller["start"], 16), int(caller["end_exclusive"], 16)
    raw = image[start - BASE:end - BASE]
    require(raw.hex() == caller["bytes"] and sha256(raw) == caller["sha256"],
            "caller bytes/hash")
    require(decode(image_path, start, end) == caller["instructions"], "fresh caller decode")
    call = 0x41C5E3F0
    require(image[call - BASE] == 0xE8 and
            call + 5 + struct.unpack_from("<i", image, call - BASE + 1)[0] == START,
            "direct caller rel32")
    print("PASS W5-ESI: pinned sources/image; 383 continuous callee instructions; "
          "5 caller instructions; 9 opaque calls; 1 local branch; 1 supported ret 4")
    print("PROVEN LOCAL: save at 0x41c5bda5; slot [frame EBP-0xc]; "
          "restore 0x41c5c70f..0x41c5c716")
    print("CONDITIONAL: normal-return traversal, EBP/ESI frame discipline and saved-slot "
          "integrity; nested effects and exception/non-local paths not proved")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    try:
        verify(args.handoff.resolve())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f"FAIL W5-ESI: {error}\n")


if __name__ == "__main__":
    main()
