#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path


BASE = 0x412A0000
START = 0x414AF0F6
END = 0x414AF1D6
SOURCE = {
    "image": "workspace/work/nl_handoff_2026-11-06/nl.bin",
    "image_sha256": "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e",
    "image_size": 55578624,
    "va_base": "0x412a0000",
    "container": "workspace/input/dll/neverlose.dll",
    "container_sha256": "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c",
    "resource_offset": "0x629b10",
    "resource_size": 55578624,
}
CODE_HEX = (
    "f30f104714f30f1145c0f30f1145d056e8107cb700f30f1045c083c4046683f8027419"
    "f30f1145d056e8f77bb700f30f104dc083c4046683f80175030f57c983ec10f20f1005"
    "c8cd0642f20f114424080f57c0f30f5ac1f20f110424e8e91eb80083c410d95db4"
    "f30f1045b4f30f100dd0cd0642f30f58c8f30f1015d4cd0642f30fc2d0010f54ca"
    "0f55d00f56d1f30f100dd8cd0642f30f58ca0f28c2f30fc205dccd0642010f54c8"
    "0f55c20f56c1f20f104f0c0f5f0d907f07420f5d0da07f0742f30f5f0510fc0642"
    "0f134f0cf30f5d0514fc0642f30f114714e9d7040000"
)
DATA = {
    0x4206CDC8: ("0000000000807640", "d", (360.0,)),
    0x4206CDD0: ("0000b4c3000034430000b443000034c3", "4f", (-360.0, 180.0, 360.0, -180.0)),
    0x42077F90: ("0000b2c2000034c30000000000000000", "4f", (-89.0, -180.0, 0.0, 0.0)),
    0x42077FA0: ("0000b242000034430000000000000000", "4f", (89.0, 180.0, 0.0, 0.0)),
    0x4206FC10: ("000034c200003442", "2f", (-45.0, 45.0)),
}
REQUIRED = {
    0x414AF113: "cmp ax,0x2",
    0x414AF117: "je 0x414af132",
    0x414AF11F: "call 0x42026d1b",
    0x414AF124: "movss xmm1,DWORD PTR [ebp-0x40]",
    0x414AF12C: "cmp ax,0x1",
    0x414AF130: "jne 0x414af135",
    0x414AF132: "xorps xmm1,xmm1",
    0x414AF149: "cvtss2sd xmm0,xmm1",
    0x414AF152: "call 0x42031040",
    0x414AF15A: "fstp DWORD PTR [ebp-0x4c]",
    0x414AF16A: "addss xmm1,xmm0",
    0x414AF176: "cmpltss xmm2,xmm0",
    0x414AF18C: "addss xmm1,xmm2",
    0x414AF193: "cmpltss xmm0,DWORD PTR ds:0x4206cddc",
    0x414AF1A5: "movsd xmm1,QWORD PTR [edi+0xc]",
    0x414AF1AA: "maxps xmm1,XMMWORD PTR ds:0x42077f90",
    0x414AF1B1: "minps xmm1,XMMWORD PTR ds:0x42077fa0",
    0x414AF1B8: "maxss xmm0,DWORD PTR ds:0x4206fc10",
    0x414AF1C0: "movlps QWORD PTR [edi+0xc],xmm1",
    0x414AF1C4: "minss xmm0,DWORD PTR ds:0x4206fc14",
    0x414AF1CC: "movss DWORD PTR [edi+0x14],xmm0",
    0x414AF1D1: "jmp 0x414af6ad",
}


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def decode(text):
    rows = []
    for line in text.splitlines():
        match = re.fullmatch(
            r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+[0-9a-f]{2}|[0-9a-f]{2})\s+([a-z].*)",
            line,
        )
        if match:
            rows.append({
                "va": hex(int(match[1], 16)),
                "bytes": bytes.fromhex(match[2]).hex(),
                "decoded": " ".join(match[3].split()),
            })
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handoff", required=True)
    handoff = Path(parser.parse_args().handoff)
    pins = json.loads(Path(__file__).with_name("source_anchors.json").read_text())
    require(pins["source"] == SOURCE, "source identity pins")
    image = handoff / SOURCE["image"]
    raw = image.read_bytes()
    dll = (handoff / SOURCE["container"]).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == SOURCE["image_sha256"], "image SHA-256")
    require(hashlib.sha256(dll).hexdigest() == SOURCE["container_sha256"], "DLL SHA-256")
    require(len(raw) == SOURCE["image_size"], "image size")
    offset = int(SOURCE["resource_offset"], 16)
    require(dll[offset:offset + SOURCE["resource_size"]] == raw, "embedded resource bytes")
    require(pins["range_start"] == hex(START) and pins["range_end_exclusive"] == hex(END),
            "contiguous range bounds")
    require(pins["model_start"] == "0x414af113", "local model boundary")
    require(pins["range_bytes"] == CODE_HEX, "range literal pin")
    require(raw[START - BASE:END - BASE] == bytes.fromhex(CODE_HEX), "contiguous body bytes")

    cursor = START
    for instruction in pins["instructions"]:
        va = int(instruction["va"], 16)
        word = bytes.fromhex(instruction["bytes"])
        require(va == cursor and word, f"instruction continuity at {cursor:#x}")
        require(raw[va - BASE:va - BASE + len(word)] == word, f"instruction bytes at {va:#x}")
        cursor += len(word)
    require(cursor == END, "complete instruction range")
    objdump = shutil.which("objdump")
    require(objdump is not None, "objdump is required")
    result = subprocess.run([
        objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel", "--insn-width=16",
        f"--adjust-vma={BASE:#x}", f"--start-address={START:#x}",
        f"--stop-address={END:#x}", str(image),
    ], capture_output=True, text=True, check=False)
    require(result.returncode == 0, f"objdump exit {result.returncode}: {result.stderr}")
    rows = decode(result.stdout)
    require(rows == pins["instructions"], "fresh complete instruction decode")
    by_va = {int(row["va"], 16): row["decoded"] for row in rows}
    for va, expected in REQUIRED.items():
        require(by_va.get(va) == expected, f"guard/arithmetic/store decode at {va:#x}")

    by_va = {int(row["va"], 16): row for row in pins["data"]}
    require(set(by_va) == set(DATA), "constant anchor set")
    for va, (word_hex, fmt, values) in DATA.items():
        word = bytes.fromhex(word_hex)
        actual = raw[va - BASE:va - BASE + len(word)]
        require(actual == word and by_va[va]["bytes"] == word_hex, f"constant bytes at {va:#x}")
        require(struct.unpack("<" + fmt, actual) == values and
                by_va[va]["decoded_values"] == list(values), f"constant values at {va:#x}")
    print(f"PASS source SHA-256: {SOURCE['image_sha256']}; DLL resource matches")
    print(f"PASS {len(rows)} fresh decoded instructions, {END - START} contiguous bytes, {len(DATA)} constant blocks")
    print("PASS distinct AX guards, CVTSS2SD/FSTP32 boundary, ordered selections and two EDI stores")
    print("LIMIT: no original loading/execution/emulation; unknown call bodies/providers remain open")


if __name__ == "__main__":
    main()
