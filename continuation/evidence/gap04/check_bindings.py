#!/usr/bin/env python3
"""Read-only byte and PE checks for the bounded GAP04 ledger."""

import argparse
import csv
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TARGET = 0x289A6D60
RAW_BASE = 0x412A0000
EXPECTED_HASHES = {
    "client.dll": "183d42fac8a4ecc24dc02edf5f09122f37fe9e7533ca093a867718b25cfdeb03",
    "engine.dll": "bcca98731eab8b72d1ef34fa5fe420f26ac98e8dd1d91698171d0ecd9935faa6",
    "neverlose.dll": "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c",
    "nl.bin": "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e",
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_pe(data, path):
    require(data[:2] == b"MZ", f"{path}: missing MZ header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[pe:pe + 4] == b"PE\0\0", f"{path}: missing PE signature")
    coff = pe + 4
    machine, section_count = struct.unpack_from("<HH", data, coff)
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    optional = coff + 20
    magic = struct.unpack_from("<H", data, optional)[0]
    require(machine == 0x14C and magic == 0x10B, f"{path}: expected PE32/x86")
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    section_table = optional + optional_size
    sections = []
    for index in range(section_count):
        offset = section_table + 40 * index
        name = data[offset:offset + 8].split(b"\0", 1)[0].decode("ascii")
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<IIII", data, offset + 8)
        sections.append((name, virtual_address, virtual_size, raw_size, raw_offset))
    return image_base, sections


def pe_bytes(data, image_base, sections, va, size):
    rva = va - image_base
    for name, section_va, virtual_size, raw_size, raw_offset in sections:
        if section_va <= rva and rva + size <= section_va + raw_size:
            offset = raw_offset + rva - section_va
            return name, offset, data[offset:offset + size]
    raise RuntimeError(f"unmapped PE range at {va:#x}+{size:#x}")


def rotate_left(value, count):
    count &= 31
    return ((value << count) | (value >> (32 - count))) & 0xFFFFFFFF


def rotate_right(value, count):
    count &= 31
    return ((value >> count) | (value << (32 - count))) & 0xFFFFFFFF


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, default=ROOT / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    workspace = args.handoff.resolve() / "workspace"
    dll_dir = workspace / "input/dll"
    image_dir = workspace / "work/nl_handoff_2026-11-06"

    image_data = {}
    for name, expected in EXPECTED_HASHES.items():
        path = image_dir / name if name == "nl.bin" else dll_dir / name
        data = path.read_bytes()
        actual = digest(data)
        require(actual == expected, f"{path}: SHA-256 mismatch: {actual}")
        image_data[name] = data
    print("PASS exact hashes: client.dll, engine.dll, neverlose.dll, nl.bin")

    pe_info = {}
    for name in ("client.dll", "engine.dll"):
        image_base, sections = read_pe(image_data[name], name)
        require(image_base == 0x10000000, f"{name}: unexpected PE ImageBase {image_base:#x}")
        pe_info[name] = (image_base, sections)

    handoff_root = args.handoff.resolve()
    orch = handoff_root / "workspace/orchestration"
    ledger = json.loads((Path(__file__).with_name("binding_ledger.json")).read_text(encoding="utf-8"))
    require(ledger["target_chain"]["value"] == f"{TARGET:#x}", "ledger target value mismatch")
    candidates = json.loads((orch / "v6/external_congruence_candidates.json").read_text(encoding="utf-8"))
    require(len(candidates) == 16, f"expected 16 V6 candidate rows, got {len(candidates)}")
    module_files = {"client": "client.dll", "engine": "engine.dll"}
    counts = Counter()
    hypothesis_bases = set()
    selected = {}
    for candidate in candidates:
        module = candidate["module"]
        require(module in module_files, f"unexpected candidate module {module}")
        filename = module_files[module]
        data = image_data[filename]
        image_base, sections = pe_info[filename]
        va = int(candidate["preferred"], 16)
        prefix = bytes.fromhex(candidate["prefix"])
        section, file_offset, actual = pe_bytes(data, image_base, sections, va, len(prefix))
        require(section == ".text" and actual == prefix, f"{filename} candidate {va:#x}: prefix mismatch")
        hypothetical_base = int(candidate["runtime_base_hypothesis"], 16)
        require(hypothetical_base + va - image_base == TARGET,
                f"{filename} candidate {va:#x}: hypothesis no longer maps to target")
        counts[module] += 1
        hypothesis_bases.add(hypothetical_base)
        selected[va] = {"module": filename, "file_offset": file_offset, "prefix": prefix.hex()}
    require(counts == Counter({"client": 14, "engine": 2}), f"unexpected candidate distribution: {counts}")
    require(len(hypothesis_bases) == 16, "candidate runtime-base hypotheses are not all distinct")
    expected_candidates = {
        0x10076D60: ("client.dll", 0x76160, "83ec10e818f68200c3cccccccccccccc"),
        0x103E6D60: ("client.dll", 0x3E6160, "f30f1165fc89b574ffffff4783ff400f"),
        0x10876D60: ("client.dll", 0x876160, "558bec83e4f883ec348b450cf30f1002"),
    }
    for va, expected in expected_candidates.items():
        row = selected.get(va)
        require(row is not None, f"missing named candidate {va:#x}")
        require((row["module"], row["file_offset"], row["prefix"]) == expected,
                f"named candidate {va:#x} metadata mismatch")
    client, client_info = image_data["client.dll"], pe_info["client.dll"]
    wrapper_call_va = 0x10076D63
    section, _, wrapper_call = pe_bytes(client, *client_info, wrapper_call_va, 5)
    require(section == ".text" and wrapper_call == bytes.fromhex("e818f68200"),
            "0x10076d60 wrapper call bytes mismatch")
    wrapper_target = wrapper_call_va + 5 + struct.unpack_from("<i", wrapper_call, 1)[0]
    require(wrapper_target == 0x108A6380, f"wrapper target is {wrapper_target:#x}")
    print("PASS PE32 byte map: all 16 V6 candidates match; 14 client, 2 engine")
    print("PASS 0x10076d60 wrapper call target: 0x108a6380")

    raw = image_data["nl.bin"]
    callsite_va = 0x414F3E19
    callsite = bytes.fromhex(
        "b806250d87 c1c00b 2ddb eb6120 c1cb20 c1ce20 c1c81a "
        "35a984a17d 2d9b269923 909090 8945d8 8b45d8 "
        "f30f11542408 8d4ddc 894c2404 8d4da8 890c24 8d4db4 ffd0"
    )
    offset = callsite_va - RAW_BASE
    require(raw[offset:offset + len(callsite)] == callsite,
            "nl.bin caller bytes differ at 0x414f3e19..0x414f3e5a")
    ledger_callsite = bytes.fromhex(ledger["target_chain"]["bytes"])
    require(ledger_callsite == callsite, "ledger callsite byte range mismatch")
    value = rotate_left(0x870D2506, 0x0B)
    value = (value - 0x2061EBDB) & 0xFFFFFFFF
    value = rotate_right(value, 0x1A)
    value ^= 0x7DA184A9
    value = (value - 0x2399269B) & 0xFFFFFFFF
    require(value == TARGET, f"caller arithmetic produced {value:#x}, expected {TARGET:#x}")
    require(callsite[-2:] == b"\xff\xd0", "expected call eax at end of verified callsite")
    require(ledger["inventories_not_closed"]["v6_candidate_rows"] == len(candidates),
            "ledger candidate inventory count mismatch")
    print("PASS nl.bin caller bytes and arithmetic: call eax target = 0x289a6d60")

    branch_va = 0x103E6D6F
    section, _, branch = pe_bytes(client, *client_info, branch_va, 6)
    require(section == ".text" and branch == bytes.fromhex("0f8e52feffff"),
            "alternate candidate loop branch bytes mismatch")
    destination = branch_va + 6 + struct.unpack_from("<i", branch, 2)[0]
    require(destination == 0x103E6BC7, f"alternate candidate branch target is {destination:#x}")
    print("PASS countercandidate branch: 0x103e6d6f -> 0x103e6bc7")

    v5_path = orch / "v5/baseline_ambiguous_bindings.csv"
    with v5_path.open(encoding="utf-8-sig", newline="") as stream:
        v5_rows = list(csv.DictReader(stream))
    require(len(v5_rows) == 14, f"expected 14 V5 ambiguous bindings, got {len(v5_rows)}")
    require(all(row.get("runtime_observed", "").lower() == "false" for row in v5_rows),
            "V5 runtime_observed inventory changed")
    v6_path = orch / "v6/external_requirements.csv"
    with v6_path.open(encoding="utf-8-sig", newline="") as stream:
        v6_rows = list(csv.DictReader(stream))
    require(len(v6_rows) == 16, f"expected 16 V6 external requirements, got {len(v6_rows)}")
    require(all("direct ragebot consumer unproven" in row.get("requirement", "") for row in v6_rows),
            "V6 external-requirement boundary changed")

    literal = struct.pack("<I", TARGET)
    literal_hits = {name: data.count(literal) for name, data in image_data.items()}
    require(not any(literal_hits.values()), f"unexpected direct target literal hits: {literal_hits}")
    print("PASS inventories: V5 ambiguous=14 (runtime_observed=false); V6 external requirements=16")
    print("PASS no direct little-endian 0x289a6d60 literal in the four hashed inputs")
    print("UNRESOLVED runtime base, module ownership, target identity, and execution/reachability")


if __name__ == "__main__":
    main()
