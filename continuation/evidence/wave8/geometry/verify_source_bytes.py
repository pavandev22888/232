#!/usr/bin/env python3
"""Verify saved source bytes only; never load or run original code."""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
import zipfile
from pathlib import Path

BASE = 0x412A0000
ARCHIVE = (149819978, "cc7fcb0e3a34d1f5b3adf884e889e9458c02158ee9823a0c45e161786920dd49")
IMAGE = (55578624, "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e")
CONTAINER_SHA = "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c"
RESOURCE_OFFSET = 0x629B10
WINDOWS = {
    "caller": (0x414F3C90, 0x414F3EB5, "72bab48ea792c8f4a906af85efe348288aaebaef9169c482df6fc596f83595cb"),
    "postcall_consumer": (0x414F3E60, 0x414F3E9F, "6530efbee3eb144ae52276436621a624393f9d16b79b681c685b13181e94efe1"),
    "return_epilogue": (0x414F3E9F, 0x414F3EB5, "318e1885b75d4024b2a32f589de788977d78a1cae064aad2b9d86b4e2aa5f796"),
    "observed_call_boundary": (0x414F3E42, 0x414F3E60, "2db85d394f361c7c6bef085c19d300e77a4aa755273b752fa7e18675453dfdd7"),
    "trace_wrapper_tail": (0x413700DC, 0x413700FC, "adcb925a6035ecc4659bda95957fef507888d663b00c2ff33c0931768ba75f19"),
}
REQUIRED_DECODE = {
    0x414F3DD0: "mov edi,DWORD PTR [ebp+0x14]",
    0x414F3E55: "lea ecx,[ebp-0x4c]",
    0x414F3E58: "call eax",
    0x414F3E60: "movss xmm0,DWORD PTR [ebp-0x4c]",
    0x414F3E65: "mov esi,0xffffffff",
    0x414F3E6A: "movss xmm1,DWORD PTR ds:0x4205a554",
    0x414F3E72: "ucomiss xmm1,xmm0",
    0x414F3E75: "jbe 0x414f3e9f",
    0x414F3E77: "mulss xmm0,DWORD PTR [edi+0x2c]",
    0x414F3E7C: "movss DWORD PTR [edi+0x2c],xmm0",
    0x414F3E81: "cmp BYTE PTR [ebp-0x2c],0x0",
    0x414F3E85: "je 0x414f3e8b",
    0x414F3E87: "mov BYTE PTR [edi+0x37],0x1",
    0x414F3E8B: "movups xmm0,XMMWORD PTR [ebp-0x48]",
    0x414F3E8F: "movups XMMWORD PTR [edi+0xc],xmm0",
    0x414F3E93: "movsd xmm0,QWORD PTR [ebp-0x38]",
    0x414F3E98: "movsd QWORD PTR [edi+0x1c],xmm0",
    0x414F3E9D: "xor esi,esi",
    0x414F3EA9: "mov eax,esi",
    0x414F3EB4: "ret",
    0x413700E6: "call DWORD PTR [eax+0x14]",
    0x413700EF: "call 0x41da7c5c",
    0x413700FB: "ret",
}


def require(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def normalize(value):
    return " ".join(value.split())


def decode(data, start, directory):
    path = directory / f"window-{start:x}.bin"
    path.write_bytes(data)
    result = subprocess.run([
        "objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel", "--insn-width=16",
        f"--adjust-vma={start:#x}", str(path),
    ], check=True, text=True, capture_output=True)
    instructions = {}
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2} )+)\s*(.*?)\s*$", line)
        if match:
            instructions[int(match[1], 16)] = (bytes.fromhex(match[2]), normalize(match[3]))
    return instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    handoff = parser.parse_args().handoff.resolve()
    pins = json.loads(Path(__file__).with_name("source_anchors.json").read_text())
    source = pins["source"]
    archive = handoff / "workspace/input/nl_handoff_2026-11-06.zip"
    require((archive.stat().st_size, file_digest(archive)) == ARCHIVE, "source archive pin")
    require((source["archive_size"], source["archive_sha256"]) == ARCHIVE, "archive metadata")
    with zipfile.ZipFile(archive) as bundle:
        raw = bundle.read("nl.bin")
    require((len(raw), digest(raw)) == IMAGE, "nested nl.bin pin")
    require((source["image_size"], source["image_sha256"]) == IMAGE, "image metadata")
    require(int(source["conditional_va_bias"], 16) == BASE, "conditional address bias")
    container = handoff / "workspace/input/dll/neverlose.dll"
    require(file_digest(container) == CONTAINER_SHA, "neverlose.dll identity")
    require(source["container_sha256"] == CONTAINER_SHA, "container metadata")
    require(int(source["resource_offset"], 16) == RESOURCE_OFFSET, "resource offset")
    with container.open("rb") as stream:
        stream.seek(RESOURCE_OFFSET)
        require(stream.read(IMAGE[0]) == raw, "container resource identity")
    print("PASS pinned ZIP/nl.bin and neverlose.dll embedded resource identity")

    require(set(pins["windows"]) == set(WINDOWS), "window inventory")
    for name, (start, end, expected_sha) in WINDOWS.items():
        entry = pins["windows"][name]
        data = raw[start - BASE:end - BASE]
        require((int(entry["start"], 16), int(entry["end_exclusive"], 16)) == (start, end), name + " bounds")
        require(int(entry["file_offset"], 16) == start - BASE, name + " file offset")
        require(entry["size"] == len(data), name + " size")
        require(data.hex() == entry["bytes"] and digest(data) == expected_sha == entry["sha256"], name + " bytes/hash")
        print(f"PASS {name}: [{start:#x},{end:#x}) SHA-256 {expected_sha}")
    require(raw[0x4205A554 - BASE:0x4205A558 - BASE] == bytes.fromhex("0000803f"), "binary32 threshold 1.0")
    require(pins["threshold"] == {"va": "0x4205a554", "bytes": "0000803f"}, "threshold metadata")

    instructions = {}
    with tempfile.TemporaryDirectory(prefix="w8-geometry-bytes-") as temporary:
        directory = Path(temporary)
        for start, end in ((0x414F3C90, 0x414F3EB5), (0x413700DC, 0x413700FC)):
            instructions.update(decode(raw[start - BASE:end - BASE], start, directory))
    anchored = {int(item["va"], 16): item for item in pins["instructions"]}
    require(set(anchored) == set(REQUIRED_DECODE), "instruction inventory")
    for va, expected_decode in REQUIRED_DECODE.items():
        actual_bytes, actual_decode = instructions[va]
        entry = anchored[va]
        require(actual_bytes.hex() == entry["bytes"], f"instruction bytes at {va:#x}")
        require(actual_decode == normalize(expected_decode) == entry["decoded"], f"instruction decode at {va:#x}")
    print(f"PASS {len(REQUIRED_DECODE)} fresh static instruction decodes and threshold bytes")
    print("BOUNDARY: external observations only; no provider identity, ABI, runtime base or full geometry proof")


if __name__ == "__main__":
    main()
