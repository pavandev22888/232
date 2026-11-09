#!/usr/bin/env python3
"""Recheck one conditional T+F4 candidate without executing the original image."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


BASE = 0x412A0000
IMAGE_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def normalize(instruction):
    return " ".join(instruction.lower().split())


def decode(image, start, end):
    output = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
         "--insn-width=16", f"--adjust-vma={BASE:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(image)],
        check=True, capture_output=True, text=True,
    ).stdout
    rows = []
    for line in output.splitlines():
        fields = line.split("\t")
        if len(fields) < 3 or not re.fullmatch(r"\s*[0-9a-f]+:", fields[0]):
            continue
        rows.append({"va": hex(int(fields[0].strip()[:-1], 16)),
                     "bytes": bytes.fromhex(fields[1]).hex(),
                     "instruction": normalize(fields[2])})
    return rows


def verify(handoff):
    ledger = json.loads(Path(__file__).with_name("anchors.json").read_text())
    image = handoff / ledger["image"]["path"]
    raw = image.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == IMAGE_SHA256, "image hash differs")
    require(len(ledger["hypotheses"]) == 1, "scope must remain one hypothesis")
    sources = {}
    for path, digest in ledger["saved_source_sha256"].items():
        data = (handoff / path).read_bytes()
        require(hashlib.sha256(data).hexdigest() == digest, f"source hash differs: {path}")
        sources[path] = json.loads(data)

    anchors = {}
    saved_count = 0
    for block in ledger["blocks"]:
        start = int(block["start_va"], 16)
        end = int(block["end_va_exclusive"], 16)
        require(BASE <= start < end <= BASE + len(raw), "invalid block bounds")
        require(hashlib.sha256(raw[start - BASE:end - BASE]).hexdigest() == block["sha256"],
                f"block hash differs: {block['id']}")
        actual = decode(image, start, end)
        require(actual == block["rows"], f"fresh decode differs: {block['id']}")
        cursor = start
        for row in actual:
            va = int(row["va"], 16)
            encoded = bytes.fromhex(row["bytes"])
            require(va == cursor, f"noncontiguous decode at {va:#x}")
            require(raw[va - BASE:va - BASE + len(encoded)] == encoded,
                    f"image bytes differ: {va:#x}")
            require(va not in anchors, f"duplicate anchor: {va:#x}")
            anchors[va] = row
            cursor += len(encoded)
            if block["saved_source"]:
                saved = sources[block["saved_source"]]["instructions"]
                require(any(int(r["va"], 16) == va and r["bytes"] == row["bytes"]
                            and normalize(r["instruction"]) == row["instruction"] for r in saved),
                        f"saved row differs: {va:#x}")
                saved_count += 1
        require(cursor == end, f"incomplete block: {block['id']}")

    def at(va, instruction):
        require(va in anchors and anchors[va]["instruction"] == normalize(instruction),
                f"operand/flow differs: {va:#x}")
        return bytes.fromhex(anchors[va]["bytes"])

    at(0x414C08AB, "mov ecx,ebp")
    call = at(0x414C08B7, "call 0x414c0a50")
    require(0x414C08B7 + 5 + int.from_bytes(call[1:], "little", signed=True) == 0x414C0A50,
            "direct caller target differs")
    for va, instruction in [
        (0x414C0877, "cmp DWORD PTR [ebp+0x18],0x0"),
        (0x414C087B, "jne 0x414c088e"),
        (0x414C088E, "cmp DWORD PTR [ebp+0x44],0x0"),
        (0x414C0892, "jne 0x414c08bc"),
        (0x414C0A5E, "call 0x41da7e70"),
        (0x414C0A63, "mov esi,esp"),
        (0x414C1DC1, "ucomiss xmm6,xmm2"),
        (0x414C1DC4, "ja 0x414c1e45"),
        (0x414C1E45, "mov ecx,DWORD PTR [esi+0xb8]"),
        (0x414C1E4B, "mov eax,DWORD PTR [ecx+0x4]"),
        (0x414C1E4E, "mov ecx,DWORD PTR [ecx+0x8]"),
        (0x414C1E51, "dec ecx"),
        (0x414C1E52, "and ecx,DWORD PTR [esi+0x78]"),
        (0x414C1E55, "mov edi,DWORD PTR [eax+ecx*4]"),
        (0x41477E96, "mov eax,DWORD PTR [edx+0xf4]"),
        (0x41477E9C, "cmp DWORD PTR [ecx+esi*1+0xf4],eax"),
        (0x41477EA3, "jne 0x41477f14"),
    ]:
        at(va, instruction)

    # Slot agreement is conditional on preservation, not an interprocedural proof.
    slot_store = at(0x414C0A9E, "mov DWORD PTR [esi+0x3c],ecx")[-1]
    slot_load = at(0x414C1E5C, "mov eax,DWORD PTR [esi+0x3c]")[-1]
    require(slot_store == slot_load == 0x3C, "receiver slot differs")
    source = at(0x414C1ECF, "movss xmm0,DWORD PTR [edi+0x24]")
    destination = at(0x414C1ED4, "movss DWORD PTR [eax+0xf4],xmm0")
    require(0x414C1ECF + len(source) == 0x414C1ED4, "load/store no longer adjacent")
    require(source[:4] == bytes.fromhex("f30f1047")
            and destination[:4] == bytes.fromhex("f30f1180"), "scalar operands differ")
    require(source[-1] == 0x24 and int.from_bytes(destination[4:], "little") == 0xF4,
            "propagation offsets differ")
    copy_rows = [anchors[va] for va in sorted(anchors) if 0x414C1E5F <= va < 0x414C1ED4]
    require(all(row["instruction"].startswith(("movss xmm0,", "movss dword ptr [eax+"))
                for row in copy_rows), "intervening receiver/value definition differs")
    census = sources["workspace/orchestration/v7/wide_member_candidates.json"]
    require(any(int(c["row"]["va"], 16) == 0x414C1ED4 and c["row"]["bytes"] == destination.hex()
                and c["width"] == 4 and c["offset"] == 0xF4
                and "orchestration/v5/shot_result_complete_walk.json" in c["sources"]
                for c in census["targetF0F4"]), "candidate provenance differs")
    return {"passed": True, "image_sha256": IMAGE_SHA256,
            "blocks_checked": len(ledger["blocks"]), "fresh_decoded_instructions": len(anchors),
            "saved_rows_rechecked": saved_count, "bounded_hypotheses_checked": 1,
            "typed_positive_t_f4_source_proved": False,
            "frontier": "U == selector T, prologue ECX / slot / lifetime / order preservation, and primary positive R+24 source",
            "original_execution_or_emulation": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    arguments = parser.parse_args()
    print(json.dumps(verify(arguments.handoff.resolve()), indent=2))
