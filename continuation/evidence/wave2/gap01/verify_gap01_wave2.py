#!/usr/bin/env python3
"""Read-only GAP01 wave-2 byte, decode and bounded alias checks."""

import argparse
import hashlib
import json
import re
import shutil
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=Path(__file__).resolve().parents[4]
                        / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    ledger = json.loads(Path(__file__).with_name("results.json").read_text())
    handoff = args.handoff.resolve()
    image = handoff / ledger["image"]["path"]
    raw = image.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    require(digest == IMAGE_SHA256, f"unexpected image SHA-256: {digest}")
    require(shutil.which("objdump") is not None, "GNU objdump is required")

    sources = {}
    for source, expected_hash in ledger["saved_source_sha256"].items():
        data = (handoff / source).read_bytes()
        require(hashlib.sha256(data).hexdigest() == expected_hash,
                f"saved-source hash differs: {source}")
        sources[source] = json.loads(data)

    anchors = {}
    saved_count = 0
    for block in ledger["blocks"]:
        start, end = int(block["start_va"], 16), int(block["end_va_exclusive"], 16)
        require(BASE <= start < end <= BASE + len(raw), f"bad bounds: {block['id']}")
        require(hashlib.sha256(raw[start - BASE:end - BASE]).hexdigest() == block["sha256"],
                f"block hash differs: {block['id']}")
        actual = decode(image, start, end)
        require(actual == block["rows"], f"fresh decode differs: {block['id']}")
        cursor = start
        for row in actual:
            va = int(row["va"], 16)
            encoded = bytes.fromhex(row["bytes"])
            require(va == cursor, f"noncontiguous decode at {row['va']}")
            require(raw[va - BASE:va - BASE + len(encoded)] == encoded,
                    f"image-byte mismatch at {row['va']}")
            anchors[va] = row
            cursor += len(encoded)
            if block.get("saved_source"):
                saved = sources[block["saved_source"]]["instructions"]
                require(any(int(x["va"], 16) == va and x["bytes"] == row["bytes"]
                            and normalize(x["instruction"]) == row["instruction"] for x in saved),
                        f"saved row differs at {row['va']}")
                saved_count += 1
        require(cursor == end, f"incomplete decode: {block['id']}")

    def at(va, instruction):
        require(va in anchors and anchors[va]["instruction"] == normalize(instruction),
                f"alias anchor differs at {va:#x}")
        return bytes.fromhex(anchors[va]["bytes"])

    # Fixed operands establish local pointer arithmetic, not runtime reachability.
    at(0x4149C74F, "mov esi,esp")
    context = at(0x4149D99D, "lea ecx,[esi+0x30]")[-1]
    output_offset = int.from_bytes(at(0x4149D963, "lea eax,[esi+0x190]")[2:], "little")
    slot = at(0x4149D969, "mov DWORD PTR [esi+0x40],eax")[-1]
    at(0x4149D99B, "je 0x4149d9a8")
    at(0x4149D9A0, "call 0x41490890")
    at(0x4149089A, "mov esi,ecx")
    at(0x414908C3, "jne 0x41490b64")
    member = at(0x41490DCF, "mov eax,DWORD PTR [esi+0x10]")[-1]
    require(slot - context == member, "caller/callee context slot does not match")
    for va, instruction in [
        (0x41490DFC, "xorps xmm0,xmm0"),
        (0x41490DFF, "movlps QWORD PTR [eax+0x30],xmm0"),
        (0x41490E03, "mov DWORD PTR [eax+0x38],0x0"),
        (0x41490E0A, "mov DWORD PTR [eax+0x40],0x0"),
        (0x41490E11, "mov BYTE PTR [eax+0x44],0x1"),
        (0x41490E15, "movlps QWORD PTR [eax+0x20],xmm0"),
    ]:
        at(va, instruction)
    point_offset = bytes.fromhex(anchors[0x41490E15]["bytes"])[-1]
    require(point_offset <= 0x26 < point_offset + 8, "QWORD does not overlap +26")
    reset_block = next(b for b in ledger["blocks"] if b["id"] == "point_reset")
    zero_sequence = [r["va"] for r in reset_block["rows"]
                     if 0x41490DFC <= int(r["va"], 16) <= 0x41490E15]
    require(zero_sequence == [hex(v) for v in
            [0x41490DFC, 0x41490DFF, 0x41490E03, 0x41490E0A, 0x41490E11, 0x41490E15]],
            "intervening zero-source sequence differs")

    literal = int.from_bytes(at(0x4148DE28, "push 0x425009b8")[1:], "little")
    at(0x4148DE2D, "call 0x41da7c6a")
    at(0x41DA7C6B, "mov ebp,esp")
    at(0x41DA7C7B, "mov esi,DWORD PTR [ebp+0x8]")
    at(0x41DA7C7E, "cmp DWORD PTR [esi],0x0")
    at(0x41DA7C81, "jne 0x41da7c8f")
    at(0x41DA7C83, "or DWORD PTR [esi],0xffffffff")
    at(0x4148DE35, "cmp DWORD PTR ds:0x425009b8,0xffffffff")
    at(0x4148DEC0, "push 0x425009b8")
    at(0x4148DEC5, "call 0x41da7ce4")
    at(0x41DA7CE5, "mov ebp,esp")
    at(0x41DA7CF4, "mov ecx,DWORD PTR ds:0x422626ec")
    at(0x41DA7CFA, "mov eax,DWORD PTR [ebp+0x8]")
    at(0x41DA7CFD, "inc ecx")
    at(0x41DA7CFE, "mov DWORD PTR ds:0x422626ec,ecx")
    at(0x41DA7D05, "mov DWORD PTR [eax],ecx")
    require(not literal <= 0x425009BC < literal + 4, "argument DWORD overlaps BC")
    census = sources["workspace/orchestration/v7/wide_member_candidates.json"]
    require(any(int(x["row"]["va"], 16) == 0x41490E15
                and x["width"] == 8 and x["offset"] == 0x20
                and "orchestration/v7/point_optional_context_extended_walk.json" in x["sources"]
                for x in census["point26"]), "saved QWORD-candidate provenance differs")
    print(json.dumps({
        "passed": True, "image_sha256": digest, "blocks_checked": len(ledger["blocks"]),
        "fresh_decoded_instructions": len(anchors), "saved_rows_rechecked": saved_count,
        "bounded_hypotheses_checked": 2, "positive_sources_proved": 0,
        "point_qword": {"overlaps_plus26": True, "written_value_on_selected_fallthrough": "zero",
                         "context_slot": hex(member),
                         "conditional_caller_stack_offsets_inclusive": [hex(output_offset + point_offset),
                                                             hex(output_offset + point_offset + 7)]},
        "g_b8_argument_writes": {"inclusive_range": [hex(literal), hex(literal + 3)],
                                 "overlaps_g425009bc": False},
        "all_three_positive_source_gates": "open",
        "scope": "Static selected bytes, decode, local operands and widths; not full CFG, imports/TLS effects, runtime state, or global absence.",
        "original_dll_loaded_executed_or_emulated": False,
    }, indent=2))


if __name__ == "__main__":
    main()
