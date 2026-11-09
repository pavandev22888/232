#!/usr/bin/env python3
"""Check one frozen static pointer-cell inventory; never execute the image."""

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def decode(path, base, start, end):
    result = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={base:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(path)],
        check=True, capture_output=True, text=True, timeout=30)
    rows = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            rows.append([hex(int(match[1], 16)), "".join(match[2].split()),
                         " ".join(match[3].split()).lower()])
    return rows


def inventory(image, base, cell):
    # These are nominated instruction starts, not a control-flow/code map.
    needle = struct.pack("<I", cell)
    position = 0
    rows = []
    while (position := image.find(needle, position)) >= 0:
        previous = image[position - 2:position]
        if image[position - 1] == 0xA1:
            distance, length, kind = 1, 5, "read"
        elif previous[0] == 0x8B and previous[1] & 0xC7 == 5:
            distance, length, kind = 2, 6, "read"
        elif previous == b"\xc7\x05":
            distance, length, kind = 2, 10, "write-immediate"
        elif previous[0] == 0x89 and previous[1] & 0xC7 == 5:
            distance, length, kind = 2, 6, "write-register"
        elif previous == b"\x39\x35":
            distance, length, kind = 2, 6, "read-compare"
        elif previous == b"\x83\x3d":
            distance, length, kind = 2, 7, "read-compare"
        elif previous[0] == 0x03 and previous[1] & 0xC7 == 5:
            distance, length, kind = 2, 6, "read-add"
        else:
            raise SystemExit(f"FAIL: unclassified literal at {base + position:#x}")
        start = position - distance
        rows.append([hex(base + position), hex(base + start), kind,
                     image[start:start + length].hex()])
        position += 1
    return rows


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=repo / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    proof = json.loads((here / "fallback_provider.json").read_text())
    roots = {"repo": repo, "handoff": args.handoff.resolve()}
    image_path = roots["handoff"] / proof["image"]["path"]
    image = image_path.read_bytes()
    require(len(image) == proof["image"]["size"], "whole image size")
    require(sha256(image) == proof["image"]["sha256"], "whole image SHA256")
    base, cell = (int(proof[key], 16) for key in ("conditional_base", "cell"))
    for source in proof["sources"]:
        require(sha256((roots[source["root"]] / source["path"]).read_bytes())
                == source["sha256"], f"source hash: {source['path']}")

    table = (here / "direct_refs.tsv").read_bytes()
    require(sha256(table) == proof["inventory"]["sha256"], "inventory table hash")
    lines = table.decode().splitlines()
    require(lines[0] == "operand_va\tinstruction_va\tkind\tbytes\tinstruction", "table header")
    expected = [line.split("\t") for line in lines[1:]]
    require(all(len(row) == 5 for row in expected), "table columns")
    require(len(expected) == proof["inventory"]["literal_count"], "literal count")
    require(inventory(image, base, cell) == [row[:4] for row in expected],
            "exhaustive overlapping literal scan / nominated encodings")
    require(dict(Counter(row[2] for row in expected)) == proof["inventory"]["counts"],
            "reference classifications")

    def check_reference(row):
        _, start, _, raw, instruction = row
        actual = decode(image_path, base, int(start, 16), int(start, 16) + len(bytes.fromhex(raw)))
        require(actual == [[start, raw, instruction]], f"reference decode at {start}")

    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(check_reference, expected))

    decoded_count = 0
    for window in proof["windows"]:
        start, end = int(window["start"], 16), int(window["end_exclusive"], 16)
        require(0 <= start - base < end - base <= len(image), "window bounds")
        require(start - base == int(window["raw_offset"], 16), "window raw offset")
        raw = image[start - base:end - base]
        require(raw.hex() == window["bytes"] and sha256(raw) == window["sha256"],
                f"window bytes/hash: {window['name']}")
        if "instructions" in window:
            require(bytes.fromhex("".join(row[1] for row in window["instructions"])) == raw,
                    f"instruction coverage: {window['name']}")
            require(decode(image_path, base, start, end) == window["instructions"],
                    f"fresh window decode: {window['name']}")
            decoded_count += len(window["instructions"])

    call = int(proof["selected_chain"]["direct_call"], 16)
    require(image[call - base] == 0xE8, "selected rel32 call opcode")
    target = call + 5 + struct.unpack_from("<i", image, call - base + 1)[0]
    require(target == int(proof["selected_chain"]["direct_target"], 16), "selected rel32 target")
    print(f"PASS fallback provider: image; {len(proof['sources'])} sources; "
          f"{len(expected)} literal references with fresh nominated decodes; "
          f"{len(proof['windows'])} hashed windows / {decoded_count} fresh instructions; rel32")
    print("COUNTS: " + json.dumps(proof["inventory"]["counts"], sort_keys=True))
    print(subprocess.check_output(["objdump", "--version"], text=True).splitlines()[0])
    print("FRONTIER: indirect provider, intervening callee contract, aliases/lifetime, "
          "and writer-to-fallback temporal identity unresolved; R03/GAP04 not closed.")


if __name__ == "__main__":
    main()
