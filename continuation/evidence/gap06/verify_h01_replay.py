#!/usr/bin/env python3
"""Read-only replay of the saved 162-instruction H01 subset."""

import argparse
import csv
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
REPLAY = Path(__file__).with_name("h01_byte_replay.json")
BASE = 0x412A0000

AUDITED_FILES = {
    "continuation/include/rage/position_records.hpp": "62de359e80095b6729b7f54d9f743c6de9a4a25eb1c2a01b19992f05409e41d6",
    "continuation/src/position_records.cpp": "55081e37b82fc41d9d396145e3104ed4306889cba602af55418a3ca6d02a7bae",
    "continuation/tests/test_position_records.cpp": "e7129e2786564d77bcffa0615e6f3957e23abd97c19069c553b94aafef1beb97",
    "continuation/evidence/byte_anchors.json": "1784806e32a2ae149eb7b463485a53691383f16c140f861a0eda81c4d40712ba",
    "continuation/evidence/SOURCE_MAP.md": "b466b805f83abe89d869418d3533af097c8987d53f9b70c9c0d36a2175f349e2",
}
WALK_FILES = {
    "position_manager_initializer_walk.json": "67db0b4e270a31334905b6e74ee8ccc52b4a22be7b867b5ebc081c9f9d82194f",
    "network_update_with_tls_tails_walk.json": "1de25a5d27c7eede50c0c1a0d3a26c27ade12e7fd406c01cb777cfd75bf968ad",
    "record_actor_enrichment_walk.json": "957adb6309aba7ce88f86208a053b406f0a65a065699cc6469a62c4968e8b653",
    "position_record_reader_walk.json": "07d1a93b7fefdd382910b17c49c2366bd737520ab6d7c022f6decfcb4a794aff",
    "position_reader_caller_anchor_walk.json": "3d5071a4d2f3eeaec1093193d3336f59f8329f6bac448205978460af957079b7",
}
ARCHIVE_FILES = {
    "orchestration/v7/verify_v7.py": "edf54449618586804747c9b90f03ae9cb02eecbad02a9c54fe9f7ac446ecb575",
    "orchestration/v7/proof_spec.json": "b7f04f0c1e6793e0c5655eb9d106ef6a10480f5ca7d9a7462f1f6b8e1dcc3aa9",
    "orchestration/v7/pe_data.py": "e1faa3f8a6f780b1e2e7a39fd66e34890490dc14195febb6c230aa7d19f0d41b",
    "orchestration/v7/historical_H01_byte_evidence.json": "0897ee504a64d723e7197faba9c584f26ba58d749bbc160d9a4b2a926ff3e94e",
    "orchestration/v7/H01_replay.json": "41b3b683cb62afa6d0fbad968b9f716039fedc1a81d277084bfe90b59c9251b9",
    "orchestration/v1/direct/reproducibility.csv": "5cb29ac06cfaea02fcf586a94d5ce614312129ae040d3ba44c456610d43ae267",
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


class PE32:
    def __init__(self, path):
        self.data = path.read_bytes()
        require(self.data[:2] == b"MZ", f"not PE: {path}")
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        require(self.data[pe:pe + 4] == b"PE\0\0", f"bad PE signature: {path}")
        section_count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        optional = pe + 24
        require(struct.unpack_from("<H", self.data, optional)[0] == 0x10B,
                f"expected PE32: {path}")
        self.image_base = struct.unpack_from("<I", self.data, optional + 28)[0]
        self.headers_size = struct.unpack_from("<I", self.data, optional + 60)[0]
        table = optional + optional_size
        self.sections = []
        for index in range(section_count):
            start = table + 40 * index
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, start + 8)
            self.sections.append((rva, raw_size, raw_offset, virtual_size))

    def file_offset(self, rva):
        if 0 <= rva < self.headers_size:
            return rva
        for section_rva, raw_size, raw_offset, _ in self.sections:
            delta = rva - section_rva
            if 0 <= delta < raw_size:
                return raw_offset + delta
        raise ValueError(f"RVA not backed by PE file: {rva:#x}")

    def read_va(self, va, size):
        offset = self.file_offset(va - self.image_base)
        if offset + size > len(self.data):
            raise ValueError(f"PE read out of range: {va:#x}")
        return self.data[offset:offset + size]


def normalize(text):
    text = re.sub(r"<[^>]*>", "", text.lower())
    text = re.sub(r"(ds|fs|gs|es|cs|ss):(0x[0-9a-f]+)", r"\1:[\2]", text)
    text = text.replace("ds:", "")
    text = re.sub(r"\*1(?=[\]\+\-])", "", text)
    parts = text.strip().split(None, 1)
    if len(parts) == 2 and parts[0] == "mov":
        operands = [part.strip() for part in parts[1].split(",")]
        registers = {"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"}
        if len(operands) == 2:
            for index in range(2):
                memory = operands[index].startswith("[") or re.match(
                    r"(fs|gs|es|cs|ss):\[", operands[index])
                if memory and operands[1 - index] in registers and "ptr" not in operands[index]:
                    operands[index] = "dword ptr " + operands[index]
            text = "mov " + ",".join(operands)
    text = re.sub(
        r"(?<![a-z0-9_])(?:0x[0-9a-f]+|[0-9]+)(?![a-z0-9_])",
        lambda match: hex(int(match[0], 16 if match[0].startswith("0x") else 10)), text)
    return re.sub(r"\s+", "", text)


def decode(va, data, objdump, temp_dir):
    path = temp_dir / "decode.bin"
    path.write_bytes(data)
    result = subprocess.run(
        [objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel",
         "--insn-width=16", "--adjust-vma=" + hex(va), str(path)],
        capture_output=True, text=True, check=True, timeout=10)
    pattern = re.compile(r"^\s*" + f"{va:x}" + r":\s*((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", re.M)
    match = pattern.search(result.stdout)
    require(match is not None, f"objdump emitted no instruction at {va:#x}")
    return "".join(match.group(1).split()), match.group(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=ROOT / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    handoff = args.handoff.resolve(strict=True)
    workspace = handoff / "workspace"
    v7 = workspace / "orchestration/v7"
    objdump = shutil.which("objdump")
    require(objdump is not None, "GNU objdump is required")

    script_hashes = {}
    for relative, expected in ARCHIVE_FILES.items():
        path = workspace / relative
        actual = sha256(path)
        require(actual == expected, f"handoff provenance hash mismatch: {relative}")
        script_hashes[relative] = actual

    audited_hashes = {}
    for relative, expected in AUDITED_FILES.items():
        path = ROOT / relative
        actual = sha256(path)
        require(actual == expected, f"audited source changed: {relative}")
        audited_hashes[relative] = actual

    walk_hashes = {}
    for name, expected in WALK_FILES.items():
        path = workspace / "orchestration/v10" / name
        actual = sha256(path)
        require(actual == expected, f"V10 walk changed: {name}")
        walk_hashes[name] = actual

    proof_spec = json.loads((v7 / "proof_spec.json").read_text(encoding="utf-8"))
    image_hashes = {}
    for item in proof_spec["input_hashes"]:
        path = workspace / item["path"]
        require(path.resolve().is_relative_to(workspace.resolve()),
                f"input escapes handoff workspace: {item['path']}")
        actual = sha256(path)
        require(actual == item["sha256"], f"historical input hash mismatch: {item['path']}")
        image_hashes[item["path"]] = actual

    archived = json.loads((v7 / "H01_replay.json").read_text(encoding="utf-8"))
    replay = json.loads(REPLAY.read_text(encoding="utf-8"))
    require(replay == archived, "checked-in byte replay differs from archived replay")
    evidence = json.loads((v7 / "historical_H01_byte_evidence.json").read_text(encoding="utf-8"))
    require(len(evidence) == len(replay["checks"]) == 162, "expected 162 H01 byte records")
    require(replay["passed"] and replay["anchors"] == 162 and
            replay["resource"] == 143 and replay["client"] == 19,
            "unexpected H01 replay summary")

    metrics_path = workspace / "work/RAGEBOT_FINAL_v2/evidence/verified_history_metrics_current.json"
    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    require(metrics["checks_total"] == 209 and metrics["checks_passed"] == 209 and
            all(item.get("pass") is True for item in metrics["checks"]),
            "archived H01 metric record is not 209/209 PASS")
    require(len(metrics["byte_evidence"]) == 162, "historical byte evidence count changed")
    historical_instruction_checks = sum(
        item.get("check", "").startswith("Original instruction ")
        for item in metrics["checks"])
    other_historical_checks = len(metrics["checks"]) - historical_instruction_checks
    require(historical_instruction_checks == 162 and other_historical_checks == 47,
            "unexpected H01 check partition")

    csv_path = workspace / "orchestration/v1/direct/reproducibility.csv"
    with csv_path.open(encoding="utf-8-sig", newline="") as stream:
        h01 = next(row for row in csv.DictReader(stream) if row["id"] == "H01")
    require(h01["historical_pass_count"] == "209" and
            h01["strict_old_protocol_repeated"] == "False" and
            h01["script_present"] == "False",
            "H01 reproducibility ledger contradicts expected partial-repeat status")

    resource = (workspace / "work/nl_handoff_2026-11-06/nl.bin").read_bytes()
    neverlose = (workspace / "input/dll/neverlose.dll").read_bytes()
    client = PE32(workspace / "input/dll/client.dll")
    temp_root = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix=".h01-decode-", dir=temp_root) as temporary:
        temp_dir = Path(temporary)
        for source, row in zip(evidence, replay["checks"]):
            va = int(source["va"], 16)
            raw = bytes.fromhex(source["bytes"])
            expected = source["mnemonic"] + " " + source["operands"]
            require(row["va"] == source["va"] and row["source"] == source["source"] and
                    row["bytes"] == source["bytes"] and row["expected"] == expected and
                    row["passed"] is True, f"replay record mismatch at {source['va']}")
            if source["source"] == "resource":
                start = va - BASE
                require(start >= 0 and resource[start:start + len(raw)] == raw,
                        f"nl.bin byte mismatch at {source['va']}")
                file_image = neverlose
                decode_window = resource[start:start + 96]
            else:
                require(source["source"] == "client", f"unknown source: {source['source']}")
                offset = int(source["file_offset"], 16)
                require(client.data[offset:offset + len(raw)] == raw,
                        f"client file-offset bytes mismatch at {source['va']}")
                require(client.file_offset(va - client.image_base) == offset,
                        f"client VA/file-offset mismatch at {source['va']}")
                decode_window = client.read_va(va, 96)
                file_image = client.data
            offset = int(source["file_offset"], 16)
            require(file_image[offset:offset + len(raw)] == raw,
                    f"source file-offset bytes mismatch at {source['va']}")
            decoded_bytes, decoded_instruction = decode(va, decode_window, objdump, temp_dir)
            require(decoded_bytes == source["bytes"], f"fresh decode byte mismatch at {source['va']}")
            require(normalize(expected) == normalize(decoded_instruction),
                    f"fresh decode instruction mismatch at {source['va']}: {decoded_instruction}")

    result = {
        "passed": True,
        "command": "python3 continuation/evidence/gap06/verify_h01_replay.py --handoff <RAGEBOT_HANDOFF_V10>",
        "h01": {
            "archivedChecks": 209,
            "archivedReportedPasses": 209,
            "storedByteAssertions": 162,
            "freshlyReplayedByteAssertions": 162,
            "resource": 143,
            "client": 19,
            "otherHistoricalChecksNotReplayed": 47,
            "strictOldProtocolRepeated": False,
        },
        "inputs": image_hashes,
        "scriptsAndEvidence": script_hashes,
        "auditedSources": audited_hashes,
        "v10Walks": walk_hashes,
        "scope": "Read-only hash, stored-byte, PE-offset and GNU objdump checks for the selected H01 subset; not a replay of all 209 checks or an independent external audit.",
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
