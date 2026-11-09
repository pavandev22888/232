"""Verify the immediate reset-table source and its complete six-iteration loop."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    proof = json.loads((here / "anchors.json").read_text())
    root = args.handoff.resolve(strict=True)
    path = (root / proof["source"]["path"]).resolve(strict=True)
    require(path.is_relative_to(root), "source outside handoff")
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == proof["source"]["sha256"],
            "image hash mismatch")
    base = 0x412a0000
    for anchor in proof["anchors"]:
        start = int(anchor["address"], 16) - base
        expected = bytes.fromhex(anchor["bytes"])
        require(raw[start:start + len(expected)] == expected, anchor["role"])

    body = bytearray()
    for offset in range(0, 0xc8, 8):
        if offset == 0:
            body += bytes.fromhex("c700")
        elif offset < 0x80:
            body += bytes.fromhex("c740") + bytes([offset])
        else:
            body += bytes.fromhex("c780") + offset.to_bytes(4, "little")
        body += bytes.fromhex("ffffffff")
    require(len(body) == 0x415decb9 - 0x415debf0, "loop instruction extent")
    require(raw[0x415debf0 - base:0x415decb9 - base] == body,
            "25 ordered DWORD stores")
    start, stride, end = 0x42502bec, 0xc8, 0x4250309c
    require((end - start) == 6 * stride, "six exact iterations")
    require(0x415decc9 + int.from_bytes(bytes.fromhex("27ffffff"),
                                      "little", signed=True) == 0x415debf0,
            "back-edge target")
    decoded = subprocess.check_output([
        "objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
        "--adjust-vma=0x412a0000", "--start-address=0x415debde",
        "--stop-address=0x415decc9", str(path)], text=True)
    for text in ("eax,0x42502bec", "eax,0xc8", "eax,0x4250309c", "0x415debf0"):
        require(text in decoded, "fresh decode: " + text)
    print("PASS: immediate EAX source, 25 ordered stores, stride/end/back-edge, six records")


if __name__ == "__main__":
    main()
