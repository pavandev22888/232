#!/usr/bin/env python3
"""Check saved bytes and static decodes; never load or execute the original."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

BASE = 0x412A0000
SIZE = 0x3501000
RESOURCE_OFFSET = 0x629B10
IMAGE_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
DLL_SHA256 = "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c"
PINS_SHA256 = "0484c3b6e8332c6c0347d8557d034461f32a0f57fbf8d3fb54ab88f39239bc7b"
TABLE_TARGETS = (
    0x41370903, 0x41370A98, 0x413709BB, 0x41370A98,
    0x41370A98, 0x413708F6, 0x413708F6, 0x41370A98,
)


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def decode(objdump, image, start, stop):
    result = subprocess.run([
        objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel",
        "--insn-width=16", f"--adjust-vma={BASE:#x}",
        f"--start-address={start:#x}", f"--stop-address={stop:#x}", str(image),
    ], capture_output=True, text=True, check=False)
    require(result.returncode == 0, f"objdump at {start:#x}: {result.stderr.strip()}")
    instructions = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$", line)
        if match:
            instructions.append({
                "va": hex(int(match[1], 16)),
                "bytes": bytes.fromhex(match[2]).hex(),
                "decoded": " ".join(match[3].split()),
            })
    return instructions


def ror(value, count):
    return ((value >> count) | (value << (32 - count))) & 0xFFFFFFFF


def rol(value, count):
    return ((value << count) | (value >> (32 - count))) & 0xFFFFFFFF


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True, type=Path)
    handoff = parser.parse_args().handoff
    manifest = Path(__file__).with_name("source_anchors.json")
    require(sha256(manifest) == PINS_SHA256, "source manifest SHA-256")
    pins = json.loads(manifest.read_text())
    source = pins["source"]
    require(source["image_sha256"] == IMAGE_SHA256, "image hash pin")
    require(source["container_sha256"] == DLL_SHA256, "container hash pin")
    require(source["va_base"] == hex(BASE), "raw VA base")
    require(source["image_size"] == SIZE == source["resource_size"], "resource size")
    require(int(source["resource_offset"], 16) == RESOURCE_OFFSET, "resource offset")
    image = handoff / source["image"]
    dll = handoff / source["container"]
    require(sha256(image) == IMAGE_SHA256, "nl.bin SHA-256")
    require(sha256(dll) == DLL_SHA256, "neverlose.dll SHA-256")
    raw = image.read_bytes()
    require(len(raw) == SIZE, "raw image length")
    with dll.open("rb") as stream:
        stream.seek(RESOURCE_OFFSET)
        require(stream.read(SIZE) == raw, "DLL resource identity")
    objdump = shutil.which("objdump")
    require(objdump is not None, "GNU objdump required")
    instructions = []
    for region in pins["regions"]:
        start, stop = int(region["start"], 16), int(region["stop"], 16)
        data = raw[start - BASE:stop - BASE]
        require(data.hex() == region["bytes"], f"range bytes at {start:#x}")
        require(hashlib.sha256(data).hexdigest() == region["sha256"],
                f"range hash at {start:#x}")
        instructions.extend(decode(objdump, image, start, stop))
    require(instructions == pins["instructions"], "all instruction decodes")
    for item in pins["data"]:
        start = int(item["va"], 16) - BASE
        expected = bytes.fromhex(item["bytes"])
        require(raw[start:start + len(expected)] == expected, f"data at {item['va']}")
    table = struct.unpack_from("<8I", raw, 0x4206DC2C - BASE)
    require(table == TABLE_TARGETS, "all eight switch targets")
    for va, expected in ((0x4206DB58, 0x3F400000), (0x4206DB5C, 0x3FA00000),
                         (0x4205A518, 0x3F000000), (0x4205A554, 0x3F800000)):
        require(struct.unpack_from("<I", raw, va - BASE)[0] == expected,
                f"binary32 constant at {va:#x}")

    # Static folding of pinned immediate-only blocks, not original-code emulation.
    guard3 = (((0xF298C73F + 0x005F4A1C + 0x20AABC98) & 0xFFFFFFFF)
              ^ 0x052FB022) - 0x168D7DB1
    guard_other = ror(0xD7EF8B2D, 15)
    for operation, count in ((rol, 23), (ror, 16), (rol, 28), (rol, 7)):
        guard_other = operation(guard_other, count)
    guard_other = ((guard_other - 0x69197050) & 0xFFFFFFFF) ^ 0x05A60C29
    require(guard3 & 0xFFFFFFFF == guard_other == 0x20, "both hitgroup-1 guard constants")
    object3 = ror((0x8AED9011 + 0x3E8C1080) & 0xFFFFFFFF, 7) ^ 0x781E460C
    object3 = (rol(ror(object3, 15), 8) - 0x6C9D982A) & 0xFFFFFFFF
    object_other = rol(0xDFD2ADEF ^ 0x70F34DED, 11) ^ 0x0740AD1F
    object_other = rol(rol(ror(object_other, 30), 8), 4)
    require((object3, object_other) == (0x2E198140, 0x2E198210),
            "distinct hitgroup-1 provider object operands")
    require(all(not BASE <= address < BASE + SIZE for address in (object3, object_other)),
            "provider object operands are outside the raw image")
    print("PASS nl.bin/DLL SHA-256 and exact embedded-resource identity")
    print(f"PASS {len(pins['regions'])} byte ranges, {len(instructions)} static instruction decodes")
    print("PASS eight switch targets, four binary32 constants, both guards == 0x20")
    print("PASS distinct provider operands 0x2e198140 / 0x2e198210; bindings remain unknown")


if __name__ == "__main__":
    main()
