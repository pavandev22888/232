#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path


RAW_BASE = 0x412A0000
IMAGE_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
DLL_SHA256 = "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c"
RESOURCE_OFFSET = 0x629B10
RESOURCE_SIZE = 0x3501000
TABLE_VA = 0x4206DC2C
TABLE_TARGETS = (
    0x41370903, 0x41370A98, 0x413709BB, 0x41370A98,
    0x41370A98, 0x413708F6, 0x413708F6, 0x41370A98,
)
MULTIPLIER_VA = 0x4206DB5C
MULTIPLIER = 1.25
REQUIRED = {
    0x4136E09E: ("e8fd1e0000", "call 0x4136ffa0"),
    0x413700E6: ("ff5014", "call DWORD PTR [eax+0x14]"),
    0x4136F0ED: ("e81e160000", "call 0x41370710"),
    0x4136F0F5: ("d99e48010000", "fstp DWORD PTR [esi+0x148]"),
    0x413708DA: ("ff5030", "call DWORD PTR [eax+0x30]"),
    0x413708DD: ("d95dd0", "fstp DWORD PTR [ebp-0x30]"),
    0x413708E0: ("4b", "dec ebx"),
    0x413708E1: ("83fb07", "cmp ebx,0x7"),
    0x413708E4: ("0f87de000000", "ja 0x413709c8"),
    0x413708EA: ("f30f104dd0", "movss xmm1,DWORD PTR [ebp-0x30]"),
    0x413708EF: ("ff249d2cdc0642", "jmp DWORD PTR [ebx*4+0x4206dc2c]"),
    0x413709BB: ("f30f590d5cdb0642", "mulss xmm1,DWORD PTR ds:0x4206db5c"),
    0x413709C3: ("e9d0000000", "jmp 0x41370a98"),
    0x41370AA1: ("f30f114dd8", "movss DWORD PTR [ebp-0x28],xmm1"),
    0x41370AA6: ("d945d8", "fld DWORD PTR [ebp-0x28]"),
    0x41370AB0: ("c3", "ret"),
}


def fail(message):
    raise SystemExit(f"FAIL: {message}")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_instruction(objdump, image, va, raw, expected):
    command = [
        objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel",
        f"--adjust-vma={RAW_BASE:#x}",
        f"--start-address={va:#x}",
        f"--stop-address={va + len(raw):#x}",
        str(image),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        fail(f"objdump failed at {va:#x}: {result.stderr.strip()}")
    line = next(
        (row for row in result.stdout.splitlines()
         if re.match(rf"^\s*{va:x}:", row, re.IGNORECASE)),
        None,
    )
    if line is None:
        fail(f"objdump emitted no instruction at {va:#x}")
    rest = line.split(":", 1)[1]
    match = re.search(r"\s{2,}([a-z][a-z0-9.]*)\s*(.*)$", rest, re.IGNORECASE)
    if match is None:
        fail(f"objdump could not decode instruction at {va:#x}: {line}")
    decoded = " ".join((match.group(1) + " " + match.group(2)).lower().split())
    if decoded != expected.lower():
        fail(f"decode at {va:#x}: {decoded!r} != {expected!r}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True)
    args = parser.parse_args()
    handoff = Path(args.handoff)
    evidence = Path(__file__).with_name("source_anchors.json")
    pins = json.loads(evidence.read_text())
    source = pins["source"]
    image = handoff / source["image"]
    dll = handoff / source["container"]

    if source["va_base"] != hex(RAW_BASE):
        fail("VA base pin")
    if sha256(image) != IMAGE_SHA256 or source["image_sha256"] != IMAGE_SHA256:
        fail("nl.bin SHA-256")
    if sha256(dll) != DLL_SHA256 or source["container_sha256"] != DLL_SHA256:
        fail("neverlose.dll SHA-256")
    if image.stat().st_size != RESOURCE_SIZE or source["image_size"] != RESOURCE_SIZE:
        fail("nl.bin size")
    if source["resource_size"] != RESOURCE_SIZE:
        fail("resource size pin")
    if int(source["resource_offset"], 16) != RESOURCE_OFFSET:
        fail("resource offset pin")
    with dll.open("rb") as stream:
        stream.seek(RESOURCE_OFFSET)
        resource = stream.read(RESOURCE_SIZE)
    raw = image.read_bytes()
    if len(resource) != RESOURCE_SIZE or resource != raw:
        fail("embedded resource does not match nl.bin")

    by_va = {}
    for item in pins["instructions"]:
        va = int(item["va"], 16)
        expected = bytes.fromhex(item["bytes"])
        offset = va - RAW_BASE
        actual = raw[offset:offset + len(expected)]
        if actual != expected:
            fail(f"bytes at {va:#x}: {actual.hex()} != {expected.hex()}")
        by_va[va] = item

    for va, (expected_bytes, expected_decode) in REQUIRED.items():
        item = by_va.get(va)
        if item is None or item["bytes"] != expected_bytes or item["decoded"] != expected_decode:
            fail(f"required source pin missing or changed at {va:#x}")
    objdump = shutil.which("objdump")
    if objdump is None:
        fail("objdump is required for instruction decoding")
    for va, (expected_bytes, expected_decode) in REQUIRED.items():
        check_instruction(
            objdump, image, va, bytes.fromhex(expected_bytes), expected_decode
        )

    table = next(item for item in pins["data"] if int(item["va"], 16) == TABLE_VA)
    table_bytes = bytes.fromhex(table["bytes"])
    actual_table = raw[TABLE_VA - RAW_BASE:TABLE_VA - RAW_BASE + len(table_bytes)]
    entries = struct.unpack("<8I", actual_table)
    if actual_table != table_bytes or entries != TABLE_TARGETS:
        fail("hitgroup jump table")
    if entries[2] != 0x413709BB:
        fail("hitgroup 3 table arm")

    constant = next(item for item in pins["data"] if int(item["va"], 16) == MULTIPLIER_VA)
    constant_bytes = bytes.fromhex(constant["bytes"])
    actual_constant = raw[MULTIPLIER_VA - RAW_BASE:MULTIPLIER_VA - RAW_BASE + 4]
    value = struct.unpack("<f", actual_constant)[0]
    if actual_constant != constant_bytes or value != MULTIPLIER or constant["float32"] != MULTIPLIER:
        fail("hitgroup 3 binary32 multiplier")

    print(f"PASS source identity: nl.bin sha256={IMAGE_SHA256}, DLL resource bytes match")
    print(f"PASS {len(pins['instructions'])} exact instruction anchors; {len(REQUIRED)} decoded by objdump")
    print("PASS hitgroup index 2 -> 0x413709bb; coefficient is binary32 1.25; return/store anchors match")


if __name__ == "__main__":
    main()
