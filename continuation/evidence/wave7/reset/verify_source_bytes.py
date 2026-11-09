#!/usr/bin/env python3
"""Verify the bounded reset projection against the handoff image bytes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


HERE = Path(__file__).resolve().parent
EVIDENCE_PATH = HERE / "reset_projection_evidence.json"
EXPECTED_IMAGE_SHA256 = (
    "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
)
EXPECTED_RANGES = [
    ("reset_body", 0x415DEBF0, 0x415DECB9, 25),
    ("predecessor", 0x415DECB9, 0x415DECC9, 3),
    ("fallthrough_prefix", 0x415DECC9, 0x415DECEE, 4),
    ("existing_local_reset", 0x415DECEE, 0x415DED53, 11),
]
INSTRUCTION_RE = re.compile(
    r"^([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$",
    re.IGNORECASE,
)


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    return sha256_bytes(path.read_bytes())


def parse_instructions(text):
    rows = []
    for line in text.splitlines():
        match = INSTRUCTION_RE.match(line)
        if not match:
            continue
        raw = bytes.fromhex(match.group(2))
        rows.append({
            "va": f"0x{int(match.group(1), 16):08x}",
            "bytes": raw.hex(),
            "size": len(raw),
            "sha256": sha256_bytes(raw),
            "decoded": " ".join(match.group(3).lower().split()),
        })
    return rows


def require_rows(actual, expected, label):
    require(len(actual) == len(expected),
            f"{label} instruction count mismatch")
    for index, (got, want) in enumerate(zip(actual, expected)):
        require(got == want,
                f"{label} instruction mismatch at row {index}: {got['va']}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True, type=Path)
    parser.add_argument("--objdump", default="objdump")
    args = parser.parse_args()

    evidence = json.loads(EVIDENCE_PATH.read_text())
    contract = evidence["contract"]
    application = contract["body_snapshot_application"]
    require(application["api"] ==
            "ResetBodySnapshot apply_reset_body_record(const ResetBodySnapshot&)",
            "raw snapshot API contract differs")
    require(application["input"]["type"] ==
            "std::array<std::uint8_t, 0xc8>" and
            application["input"]["caller_provided"] and
            not application["input"]["input_mutated"],
            "raw snapshot input contract differs")
    require(application["stores"] == {
        "count": 25,
        "width_bytes": 4,
        "value": "0xffffffff",
        "first_offset": "0x00",
        "stride_bytes": 8,
        "last_offset": "0xc0",
    }, "raw snapshot store contract differs")
    require(application["preserved_bytes"] == {
        "between_store_gaps": "offsets +0x4..+0x7 for every 8-byte slot",
        "record_tail": "0xc4..0xc7",
    }, "raw snapshot preservation contract differs")
    trace = contract["body_store_trace"]
    require(trace["separate_from_raw_snapshot_application"] and
            "does not read or mutate raw record bytes" in trace["semantics"],
            "body trace must remain distinct from raw snapshot mutation")

    image_info = evidence["image"]
    require(image_info["sha256"] == EXPECTED_IMAGE_SHA256,
            "evidence image pin differs from accepted source")
    handoff_root = args.handoff.resolve() / "workspace"
    image = handoff_root / image_info["path"]
    require(image.is_file(), f"handoff image missing: {image_info['path']}")
    require(sha256_file(image) == EXPECTED_IMAGE_SHA256,
            "handoff image SHA256 mismatch")
    image_data = image.read_bytes()
    image_base = int(image_info["image_base"], 16)

    decoded_info = evidence["decoded_evidence"]
    decoded_path = HERE / decoded_info["path"]
    require(decoded_path.is_file(), "saved disassembly evidence missing")
    require(sha256_file(decoded_path) == decoded_info["sha256"],
            "saved disassembly SHA256 mismatch")
    saved_rows = parse_instructions(decoded_path.read_text())
    manifest_rows = evidence["instructions"]
    require_rows(saved_rows, manifest_rows, "saved objdump")

    range_rows = evidence["ranges"]
    require(len(range_rows) == len(EXPECTED_RANGES),
            "unexpected source range count")
    for source_range, expected in zip(range_rows, EXPECTED_RANGES):
        name, start, end, count = expected
        require(source_range["name"] == name,
                f"unexpected range name: {source_range['name']}")
        require(int(source_range["start_va"], 16) == start and
                int(source_range["end_va_exclusive"], 16) == end,
                f"{name} boundary mismatch")
        rows = [
            row for row in manifest_rows
            if start <= int(row["va"], 16) < end
        ]
        require(len(rows) == count, f"{name} instruction count mismatch")
        cursor = start
        raw_span = bytearray()
        for row in rows:
            va = int(row["va"], 16)
            raw = bytes.fromhex(row["bytes"])
            require(va == cursor, f"{name} has a gap/overlap at {row['va']}")
            require(row["size"] == len(raw), f"{name} size mismatch at {row['va']}")
            require(row["sha256"] == sha256_bytes(raw),
                    f"{name} instruction hash mismatch at {row['va']}")
            offset = va - image_base
            require(offset >= 0 and image_data[offset:offset + len(raw)] == raw,
                    f"source bytes mismatch at {row['va']}")
            raw_span.extend(raw)
            cursor += len(raw)
        require(cursor == end, f"{name} does not end at its declared boundary")
        require(source_range["sha256"] == sha256_bytes(raw_span),
                f"{name} source span SHA256 mismatch")

    body = [row for row in manifest_rows
            if 0x415DEBF0 <= int(row["va"], 16) < 0x415DECB9]
    require(len(body) == 25, "reset body must contain 25 dword stores")
    for index, row in enumerate(body):
        offset = index * 8
        operand = "[eax]" if offset == 0 else f"[eax+0x{offset:x}]"
        require(row["decoded"] ==
                f"mov dword ptr {operand},0xffffffff",
                f"unexpected reset body store at {row['va']}")

    predecessor = [row for row in manifest_rows
                   if 0x415DECB9 <= int(row["va"], 16) < 0x415DECC9]
    require([row["bytes"] for row in predecessor] == [
        "05c8000000", "3d9c305042", "0f8527ffffff"],
        "ADD/CMP/JNE bytes differ")
    require([row["decoded"] for row in predecessor] == [
        "add eax,0xc8", "cmp eax,0x4250309c", "jne 0x415debf0"],
        "ADD/CMP/JNE decode differs")

    prefix = [row for row in manifest_rows
              if 0x415DECC9 <= int(row["va"], 16) < 0x415DECEE]
    require([row["decoded"] for row in prefix] == [
        "mov dword ptr ds:0x424fdda8,0xffffffff",
        "mov dword ptr ds:0x424f4b7c,0x0",
        "mov dword ptr ds:0x425008cc,0xffffffff",
        "mov byte ptr ds:0x425009e9,0x0",
    ], "fallthrough prefix decode/order differs")

    objdump = subprocess.run(
        [
            args.objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel",
            "--insn-width=16", f"--adjust-vma={image_info['image_base']}",
            f"--start-address={range_rows[0]['start_va']}",
            f"--stop-address={range_rows[-1]['end_va_exclusive']}",
            image_info["path"],
        ],
        cwd=handoff_root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    require_rows(parse_instructions(objdump), manifest_rows, "fresh objdump")
    print("PASS: raw snapshot mutation contract distinct from metadata trace")

    print(
        "PASS: pinned image SHA256; 43 exact instruction bytes/decodes; "
        "body 25, predecessor 3, prefix 4, existing local reset 11"
    )
    print(
        "UNRESOLVED: EAX provider/identity, initial table base, entry reachability, "
        "outer scheduling and reset-field semantics"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
