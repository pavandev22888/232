#!/usr/bin/env python3
"""Recheck frozen static bytes only; never load or execute the original image."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalized(instruction):
    return " ".join(instruction.lower().split())


def decode(image, base, start, end):
    output = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
         "--insn-width=16", f"--adjust-vma={base:#x}",
         f"--start-address={start:#x}", f"--stop-address={end:#x}", str(image)],
        check=True, capture_output=True, text=True, timeout=30).stdout
    rows = []
    for line in output.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", line)
        if match:
            rows.append([hex(int(match[1], 16)), "".join(match[2].split()),
                         normalized(match[3])])
    return rows


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    handoff = parser.parse_args().handoff.resolve(strict=True)
    proof = json.loads((here / "consumer_frontier.json").read_text())
    image_path = handoff / proof["image"]["path"]
    raw = image_path.read_bytes()
    require(len(raw) == proof["image"]["bytes"], "image size")
    require(digest(raw) == proof["image"]["sha256"], "image SHA256")
    base = int(proof["conditional_image_base"], 16)
    sources = {}
    for source in proof["sources"]:
        if source["root"] == "git":
            data = subprocess.check_output(
                ["git", "show", f"{source['revision']}:{source['path']}"], cwd=repo)
        else:
            require(source["root"] == "handoff", "source root")
            path = (handoff / source["path"]).resolve(strict=True)
            require(path.is_relative_to(handoff), "source outside handoff")
            data = path.read_bytes()
        require(len(data) == source["bytes"], f"source size: {source['path']}")
        require(digest(data) == source["sha256"], f"source SHA256: {source['path']}")
        sources[source["id"]] = json.loads(data)
    require(any(item["sha256"] == proof["image"]["sha256"] and
                "workspace/" + item["path"] == proof["image"]["path"]
                for item in sources["archive_proof"]["inputs"]),
            "image hash not supported by pinned archive manifest")

    table = (here / "byte_anchors.tsv").read_bytes()
    require(digest(table) == proof["anchors_sha256"], "anchor table SHA256")
    lines = table.decode().splitlines()
    require(lines[0] == "span\tva\tbytes\tinstruction", "anchor table header")
    rows = [line.split("\t") for line in lines[1:]]
    require(all(len(row) == 4 for row in rows), "anchor columns")
    require(set(row[0] for row in rows) == {span["id"] for span in proof["spans"]},
            "span IDs")
    total = 0
    for span in proof["spans"]:
        start, end = (int(span[key], 16) for key in ("start", "end_exclusive"))
        require(base <= start < end <= base + len(raw), "span range")
        require(digest(raw[start - base:end - base]) == span["sha256"], "span SHA256")
        expected = [row[1:] for row in rows if row[0] == span["id"]]
        require(len(expected) == span["instructions"], "span instruction count")
        require(decode(image_path, base, start, end) == expected, "fresh decode differs")
        cursor = start
        for va, byte_string, instruction in expected:
            instruction_bytes = bytes.fromhex(byte_string)
            require(int(va, 16) == cursor, "noncontiguous instructions")
            require(raw[cursor - base:cursor - base + len(instruction_bytes)] ==
                    instruction_bytes, f"original bytes: {va}")
            cursor += len(instruction_bytes)
            if "archive_source" in span:
                require(any(row["va"] == va and row["bytes"] == byte_string and
                            normalized(row["instruction"]) == instruction
                            for row in sources[span["archive_source"]]["instructions"]),
                        f"archival anchor differs: {va}")
        require(cursor == end, "span end")
        if span["id"] == "candidate_body":
            calls = [[va, instruction.split()[1]] for va, _, instruction in expected
                     if instruction.startswith("call ")]
            require(calls == proof["candidate_body"]["unfollowed_calls"], "call frontier")
            require(expected[-1] == ["0x414ab6f6", "c20400", "ret 0x4"], "normal return")
            starts = {va for va, _, _ in expected}
            branches = [instruction.split()[1] for _, _, instruction in expected
                        if instruction.startswith("j")]
            require(all(target in starts for target in branches), "nonlocal branch target")
            offsets = {hex(int(offset, 16)) if offset else "0x0"
                       for _, _, instruction in expected
                       for offset in re.findall(r"\[esi(?:\+0x([0-9a-f]+))?\]", instruction)}
            require(sorted(offsets, key=lambda offset: int(offset, 16)) ==
                    proof["candidate_body"]["observed_esi_relative_offsets"],
                    "displayed ESI-relative offsets")
        total += len(expected)

    frozen = (json.dumps(proof["hypothesis"], indent=2) + "\n").encode()
    require(digest(frozen) == proof["hypothesis_sha256"], "frozen hypothesis")
    print(f"PASS: {len(proof['sources'])} sources, {len(proof['spans'])} spans, "
          f"{total} freshly decoded instructions; static consistency only; GAP02 open")


if __name__ == "__main__":
    main()
