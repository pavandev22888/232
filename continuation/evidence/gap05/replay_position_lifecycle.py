#!/usr/bin/env python3
"""Replay GAP05 image bytes and fresh disassembly without loading the image."""

import hashlib
import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def fail(message):
    raise SystemExit(f"FAIL: {message}")


def main():
    script = Path(__file__).resolve()
    repo = script.parents[3]
    handoff = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo / "runs/handoff/RAGEBOT_HANDOFF_V10"
    evidence = json.loads((script.parent / "position_lifecycle.json").read_text())
    image_path = handoff / evidence["image"]["handoff_relative_path"]
    image = image_path.read_bytes()
    actual_hash = hashlib.sha256(image).hexdigest()
    if actual_hash != evidence["image"]["sha256"]:
        fail(f"image SHA-256 mismatch: {actual_hash}")
    if len(image) != evidence["image"]["size"]:
        fail(f"image length mismatch: {len(image)}")
    base = int(evidence["image"]["image_base"], 16)

    checked = 0
    direct_marker_writes = []
    write_mnemonics = {
        "mov", "movss", "movups", "movaps", "movlps", "movq", "add",
        "sub", "and", "or", "xor", "inc", "dec", "xchg"
    }
    for walk in evidence["source_walks"][:2]:
        data = json.loads((handoff / walk["path"]).read_text())
        if len(data["instructions"]) != walk["instruction_records"]:
            fail(f"instruction count changed: {walk['path']}")
        for instruction in data["instructions"]:
            va = int(instruction["va"], 16)
            expected = bytes.fromhex(instruction["bytes"])
            offset = va - base
            if offset < 0 or offset + len(expected) > len(image):
                fail(f"instruction outside raw image: {instruction['va']}")
            if image[offset:offset + len(expected)] != expected:
                fail(f"walk byte mismatch at {instruction['va']}")
            checked += 1
            mnemonic, _, operands = instruction["instruction"].lower().partition(" ")
            destination = operands.split(",", 1)[0]
            register = "edi" if "enrichment" in walk["path"] else "ebp"
            if mnemonic in write_mnemonics and re.search(
                    rf"\[\s*{register}\s*\+\s*0x0*48\s*\]", destination):
                direct_marker_writes.append({
                    "va": instruction["va"],
                    "instruction": instruction["instruction"]
                })

    if direct_marker_writes != evidence["producer_scan"]["direct_writes_found"]:
        fail(f"N+48 direct-write scan changed: {direct_marker_writes}")

    manager_path = handoff / evidence["source_walks"][2]["path"]
    manager_refs = json.loads(manager_path.read_text())
    if len(manager_refs) != evidence["source_walks"][2]["reference_anchors"]:
        fail("manager reference count changed")
    manager_instructions = {
        instruction["va"]: instruction
        for row in manager_refs for instruction in row.get("instructions", [])
    }
    for anchor in evidence["anchors"]:
        va = int(anchor["va"], 16)
        expected = bytes.fromhex(anchor["bytes"])
        offset = va - base
        if image[offset:offset + len(expected)] != expected:
            fail(f"anchor byte mismatch at {anchor['va']}")
    for address in ("0x414f69e4", "0x4150537f", "0x41505483"):
        if address not in manager_instructions:
            fail(f"missing manager reference {address}")
        if manager_instructions[address]["bytes"] != next(
                a["bytes"] for a in evidence["anchors"] if a["va"] == address):
            fail(f"manager reference byte mismatch at {address}")

    constants = [evidence["derived_constant"], *evidence["marker_constants"]]
    for constant in constants:
        offset = int(constant["address"], 16) - base
        raw = image[offset:offset + 4]
        if raw.hex() != constant["bytes"]:
            fail(f"constant bytes mismatch at {constant['address']}")
        if struct.unpack("<f", raw)[0] != constant["float"]:
            fail(f"constant float mismatch at {constant['address']}")

    objdump = subprocess.run(["objdump", "--version"], capture_output=True, text=True)
    if objdump.returncode:
        fail("GNU objdump is unavailable")
    repo_runs = repo / "runs/gap05"
    repo_runs.mkdir(parents=True, exist_ok=True)
    decoded = 0
    with tempfile.TemporaryDirectory(prefix="replay-", dir=repo_runs) as temp:
        temp_dir = Path(temp)
        for index, window in enumerate(evidence["decode_windows"]):
            start = int(window["start"], 16)
            end = int(window["end_exclusive"], 16)
            raw = image[start - base:end - base]
            snippet = temp_dir / f"window-{index}.bin"
            snippet.write_bytes(raw)
            result = subprocess.run(
                ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                 f"--adjust-vma={window['start']}", str(snippet)],
                capture_output=True, text=True)
            if result.returncode:
                fail(f"objdump failed for {window['start']}: {result.stderr.strip()}")
            lines = {}
            for line in result.stdout.splitlines():
                match = re.match(r"\s*([0-9a-f]+):", line, re.IGNORECASE)
                if match and "\t" in line:
                    address = "0x" + match.group(1).lower()
                    lines[address] = " ".join(line.split("\t")[-1].lower().split())
            for address, expected in window["instructions"]:
                actual = lines.get(address)
                if actual != expected.lower():
                    fail(f"decode mismatch at {address}: {actual!r} != {expected!r}")
                decoded += 1

    print(f"PASS: sha256={actual_hash}; walk instruction bytes={checked}; "
          f"fresh decoded instructions={decoded}; direct marker stores="
          f"{len(direct_marker_writes)}; manager refs={len(manager_refs)}")


if __name__ == "__main__":
    main()
