#!/usr/bin/env python3
"""Read-only checks of one frozen handle chain, not a runtime resolver."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[4]
BASE = 0x412A0000


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def ror32(value, count):
    return ((value >> count) | (value << (32 - count))) & 0xFFFFFFFF


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    handoff = args.handoff.resolve()
    ledger = json.loads(Path(__file__).with_name("anchors.json").read_text())
    roots = {"repo": ROOT, "handoff": handoff}
    sources = {}
    for source in ledger["sources"]:
        data = (roots[source["root"]] / source["path"]).read_bytes()
        require(digest(data) == source["sha256"], f"source hash: {source['path']}")
        sources[source["id"]] = data
    print(f"PASS {len(sources)} pinned source hashes")

    image = sources["nl"]
    image_path = handoff / ledger["sources"][0]["path"]
    rows = {}
    for window in ledger["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        offset = start - BASE
        require(offset == int(window["file_offset"], 16), "raw address convention")
        actual = image[offset:offset + end - start]
        require(actual.hex() == window["bytes"] and digest(actual) == window["sha256"],
                f"window bytes/hash: {window['id']}")
        decoded = decode(image_path, start, end)
        require(decoded == window["instructions"], f"fresh objdump: {window['id']}")
        rows.update({int(va, 16): (code, instruction) for va, code, instruction in decoded})
    print("PASS 2 byte windows and fresh objdump instruction boundaries")

    def expect(va, code, instruction):
        require(rows.get(va) == (code, instruction), f"anchor {va:#x}")

    for va, code in ledger["reused_boundary_anchors"]:
        offset = int(va, 16) - BASE
        require(image[offset:offset + len(bytes.fromhex(code))].hex() == code,
                f"reused entity/record boundary: {va}")

    guard = ror32(0xBD2C7ECB, 8) ^ 0x489493E8
    guard = (guard - 0x43F4248A - 0x6A880AA7 + 0x2B526FBB) & 0xFFFFFFFF
    require(guard == 0x20 == int(ledger["guard_sum"], 16), "guard constant arithmetic")
    expect(0x413130F1, "64a130000000", "mov eax,fs:0x30")
    expect(0x41313129, "c1ca20", "ror edx,0x20")
    expect(0x41313138, "3955f0", "cmp dword ptr [ebp-0x10],edx")
    expect(0x4131313B, "7427", "je 0x41313164")
    expect(0x41313161, "fa", "cli")
    expect(0x41313162, "ffe7", "jmp edi")

    address = ((0xFFAE1EE8 + 0x53F46B3B) & 0xFFFFFFFF) ^ 0x6A76E3D6 ^ 0x79FB6C6F
    address = ror32(ror32(address, 5) ^ 0x3366D2A2, 27)
    require(address == 0x2CF551DC == int(ledger["object_address_constant"], 16),
            "object-address constant arithmetic")
    expect(0x4131318D, "50", "push eax")
    expect(0x4131318E, "89e2", "mov edx,esp")
    expect(0x41313190, "8b09", "mov ecx,dword ptr [ecx]")
    expect(0x41313192, "890a", "mov dword ptr [edx],ecx")
    expect(0x41313194, "8b10", "mov edx,dword ptr [eax]")
    expect(0x41313196, "89c1", "mov ecx,eax")
    expect(0x41313198, "ff5210", "call dword ptr [edx+0x10]")
    expect(0x4131319B, "89c6", "mov esi,eax")
    expect(0x413131A7, "89f0", "mov eax,esi")
    expect(0x413131AE, "c3", "ret")
    print("PASS guarded static address 0x2cf551dc -> [vtable+0x10]; ownership unresolved")

    for va in (0x414F8567, 0x414F8573):
        code = bytes.fromhex(rows[va][0])
        require(code[0] == 0xE8 and va + 5 + struct.unpack_from("<i", code, 1)[0] == 0x413130E0,
                f"direct handle call target at {va:#x}")
    expect(0x414F856C, "85c0", "test eax,eax")
    expect(0x414F856E, "7437", "je 0x414f85a7")
    expect(0x414F8578, "89c6", "mov esi,eax")
    expect(0x414F8595, "0f8f5f030000", "jg 0x414f88fa")
    expect(0x414F859B, "a128004b42", "mov eax,ds:0x424b0028")
    expect(0x414F85A0, "0fbf0430", "movsx eax,word ptr [eax+esi*1]")
    expect(0x414F85A4, "89471c", "mov dword ptr [edi+0x1c],eax")
    print("PASS distinct repeated calls; first-zero skip; signed WORD -> N+1C DWORD")

    candidates = json.loads(sources["v6_candidates"])
    require(len(candidates) == 16, "existing runtime-base alternative count")
    require(ledger["runtime_base_alternatives"] == [
        [row["module"], row["preferred"], row["runtime_base_hypothesis"]]
        for row in candidates], "runtime-base alternatives transcription")
    require(all(int(row["runtime_base_hypothesis"], 16) +
                int(row["preferred"], 16) - 0x10000000 == 0x289A6D60
                for row in candidates), "conditional base/RVA relation")
    print("PASS 16 existing geometry alternatives retained; no handle binding established")
    print("UNRESOLVED object/vtable ownership, method target/ABI, entity supplier/index, "
          "metadata offsets, guards, repeated-call identity/lifetime and reachability")


if __name__ == "__main__":
    main()
