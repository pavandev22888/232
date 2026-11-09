#!/usr/bin/env python3
"""Read-only W5-POINT26 selected-byte and local carry-condition checks."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path


EVIDENCE = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "gap01_wave2", EVIDENCE / "wave2/gap01/verify_gap01_wave2.py")
gap01 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gap01)
require = gap01.require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    ledger = json.loads((here / "anchors.json").read_text())
    hypothesis = json.loads((here / "hypothesis.json").read_text())
    require(hypothesis["selected_field"] == "Point+0x26", "wrong field")
    require(hypothesis["hypothesis"]["id"] == ledger["hypothesis_id"], "wrong hypothesis")
    require(hypothesis["recorded_before_new_source_research"], "missing preregistration")
    handoff = args.handoff.resolve()
    image = handoff / ledger["image"]["path"]
    raw = image.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    require(digest == gap01.IMAGE_SHA256 == ledger["image"]["sha256"], "image hash differs")
    require(int(ledger["image"]["conditional_base"], 16) == gap01.BASE, "base differs")
    sources = {}
    for name, pin in ledger["saved_sources"].items():
        data = (handoff / name).read_bytes()
        require(hashlib.sha256(data).hexdigest() == pin, f"saved source differs: {name}")
        sources[name] = json.loads(data)["instructions"]
    anchors = {}
    count = 0
    for block in ledger["blocks"]:
        start, end = int(block["start_va"], 16), int(block["end_va_exclusive"], 16)
        require(gap01.BASE <= start < end <= gap01.BASE + len(raw), "bad block bounds")
        window = raw[start - gap01.BASE:end - gap01.BASE]
        require(hashlib.sha256(window).hexdigest() == block["sha256"], "window hash differs")
        decoded = gap01.decode(image, start, end)
        require(decoded == block["rows"], f"fresh decode differs: {block['id']}")
        cursor = start
        for row in decoded:
            va, encoded = int(row["va"], 16), bytes.fromhex(row["bytes"])
            require(va == cursor, "noncontiguous block")
            require(raw[va - gap01.BASE:va - gap01.BASE + len(encoded)] == encoded,
                    f"bytes differ at {va:#x}")
            require(any(int(saved["va"], 16) == va and saved["bytes"] == row["bytes"]
                        and gap01.normalize(saved["instruction"]) == row["instruction"]
                        for saved in sources[block["saved_source"]]),
                    f"saved row differs at {va:#x}")
            if va in anchors:
                require(anchors[va] == row, "conflicting duplicate start")
            anchors[va] = row
            cursor += len(encoded)
            count += 1
        require(cursor == end, "incomplete block")

    def at(va, instruction):
        require(anchors[va]["instruction"] == gap01.normalize(instruction),
                f"operand differs at {va:#x}")
        return bytes.fromhex(anchors[va]["bytes"])

    at(0x4149BB51, "jae 0x4149bb7a")
    at(0x4149BB72, "call 0x414ae650")
    at(0x4149BB77, "mov ebx,DWORD PTR [esi+0xc]")
    pointer = at(0x4149BB7A, "mov eax,ds:0x424b004c")
    slot = at(0x4149BB7F, "mov DWORD PTR [esi+0x3c],eax")
    at(0x4149BB82, "mov DWORD PTR [esi+0xc8],0x2")
    at(0x4149BB8C, "cmp ebx,0x41")
    skip = at(0x4149BB8F, "jb 0x4149bba9")
    at(0x4149BB91, "pushf")
    add = at(0x4149BB92, "add DWORD PTR [eax+0x25],0x6a")
    require(pointer[0] == 0xA1 and int.from_bytes(pointer[1:], "little") == 0x424B004C,
            "global-cell load differs")
    require(slot == bytes.fromhex("89463c"), "pointer slot differs")
    require(0x4149BB8F + len(skip) + skip[-1] == 0x4149BBA9, "unsigned skip differs")
    require(add == bytes.fromhex("8340256a"), "DWORD ADD encoding differs")
    offset, immediate, width = add[2], add[3], 4
    require(offset <= 0x26 < offset + width, "ADD does not overlap +26")
    require(immediate < 0x80, "sign-extended immediate is not positive")
    # This is byte-position arithmetic, not an interpreter or an original-path run.
    threshold = 0x100 - immediate
    require(threshold == 0x96, "carry threshold differs")
    require(threshold - 1 + immediate == 0xFF, "below-threshold boundary differs")
    require(threshold + immediate == 0x100, "carry boundary differs")
    at(0x4149BB96, "jg 0x4149bb30")
    at(0x4149BB98, "pop esp")
    at(0x4149BB9B, "jg 0x4149bb67")
    prior_store = at(0x4149BB5E, "mov DWORD PTR [esi+0xc8],0x2")
    require(0x4149BB5E < 0x4149BB67 < 0x4149BB5E + len(prior_store),
            "overlapping branch target differs")
    at(0x4149BBA6, "jne 0x4149bc1f")
    at(0x4149BBA8, "movs BYTE PTR es:[edi],BYTE PTR ds:[esi]")
    at(0x4149BBA9, "push ebx")
    at(0x4149BBAA, "call 0x41343b20")
    at(0x4149BBBF, "mov edi,DWORD PTR [esi+0x3c]")
    at(0x4149BBC2, "mov edi,DWORD PTR [edi+ecx*1+0x10930]")
    print(json.dumps({
        "passed": True,
        "image_sha256": digest,
        "blocks_checked": len(ledger["blocks"]),
        "saved_rows_and_fresh_decodes": count,
        "unique_instruction_starts": len(anchors),
        "selected_hypotheses": 1,
        "write_offsets_inclusive": [hex(offset), hex(offset + width - 1)],
        "local_alias_on_selected_fallthrough": "EAX retains DWORD[G424B004C] through ADD; that value was also stored to DWORD[ESI+0x3C]",
        "selected_edge_condition": "unsigned EBX >= 0x41 bypasses JB and reaches PUSHF before ADD, if no fault",
        "conditional_byte_formula": "new26 = (old26 + (old25 >= 0x96)) modulo 256",
        "conditional_zero_to_nonzero": "old26 == 0 and old25 >= 0x96 implies new26 == 1 at this ADD",
        "frontier": [
            "No proof that loaded G424B004C pointee is the consumed Point.",
            "No positive byte+25 source or actual pre-ADD byte+26 value established.",
            "No full CFG/reachability or nonalias proof for earlier stores, PUSHF, or later calls; first-positive ordering is not established.",
            "No same-object/lifetime/order bridge to the Point+26 -> T+96 propagation."
        ],
        "positive_point_sources_proved": 0,
        "gap01_status": "open",
        "original_dll_loaded_executed_or_emulated": False
    }, indent=2))


if __name__ == "__main__":
    main()
