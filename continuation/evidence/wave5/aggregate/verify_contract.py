"""Verify the bounded T+F0 contract against read-only archival bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def normalized(instruction):
    return re.sub(r"\s+", "", instruction.split("#")[0]).lower()


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    parser.add_argument("--scratch", type=Path, default=repo / "runs/w5-aggregate")
    args = parser.parse_args()
    handoff = args.handoff.resolve(strict=True)
    scratch = args.scratch.resolve()
    require(not scratch.is_relative_to(handoff), "Scratch must be outside read-only handoff")
    scratch.mkdir(parents=True, exist_ok=True)
    proof = json.loads((here / "anchors.json").read_text())
    contract = json.loads((here / "contract.json").read_text())
    blobs = {}
    for pin in [proof["image"], *proof["sources"]]:
        path = (handoff / pin["path"]).resolve(strict=True)
        require(path.is_relative_to(handoff), "Source outside handoff")
        data = path.read_bytes()
        require(len(data) == pin["size"], "Source size: " + pin["path"])
        require(hashlib.sha256(data).hexdigest() == pin["sha256"],
                "Source SHA256: " + pin["path"])
        blobs[pin["path"]] = data
    assignment = json.loads(subprocess.check_output(
        ["git", "show", contract["assignment_commit"] +
         ":continuation/evidence/wave5/assignment.json"], cwd=repo))
    entry = next(x for x in assignment["tasks"] if x["id"] == contract["task_id"])
    require((entry["owner"], entry["base_commit"]) ==
            (contract["owner"], contract["base_commit"]), "Assignment differs")
    raw = blobs[proof["image"]["path"]]
    base = int(proof["image"]["base"], 16)
    archives = {pin["path"]: json.loads(blobs[pin["path"]])
                for pin in proof["sources"] if pin["path"].endswith("_walk.json")}
    seen = set()
    with tempfile.TemporaryDirectory(prefix="static_", dir=scratch) as directory:
        window = Path(directory) / "instructions.bin"
        for row in proof["instructions"]:
            va = int(row["va"], 16)
            expected = bytes.fromhex(row["bytes"])
            require(va not in seen, "Duplicate VA")
            seen.add(va)
            archived = {key: row[key] for key in ("va", "bytes", "size", "instruction")}
            require(archived in archives[row["source"]]["instructions"],
                    "Archival row: " + row["va"])
            offset = va - base
            require(0 <= offset <= len(raw) - len(expected), "VA outside image")
            require(len(expected) == row["size"] and
                    raw[offset:offset + len(expected)] == expected, "Bytes: " + row["va"])
            window.write_bytes(raw[offset:offset + 96])
            output = subprocess.check_output(
                ["objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
                 "--insn-width=16", "--adjust-vma=" + hex(va), str(window)], text=True)
            decoded = None
            for line in output.splitlines():
                match = re.match(r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)\s*(.*)$", line)
                if match:
                    decoded = (int(match[1], 16), "".join(match[2].split()),
                               normalized(match[3]))
                    break
            require(decoded == (va, row["bytes"], normalized(row["instruction"])),
                    "Fresh GNU decode: " + row["va"] + " " + str(decoded))
    constant = proof["constant"]
    offset = int(constant["va"], 16) - base
    require(raw[offset:offset + 4].hex() == constant["bytes"], "Constant bytes")
    require(int.from_bytes(bytes.fromhex(constant["bytes"]), "little") ==
            int(constant["binary32_bits"], 16), "Constant binary32 bits")
    anchor_vas = {hex(va) for va in seen} | {constant["va"]}
    for claim in contract["proven_local"] + contract["unresolved"]:
        require(set(claim.get("anchors", [])).issubset(anchor_vas),
                "Contract references an unverified anchor")
    for pin in [proof["image"], *proof["sources"]]:
        require(hashlib.sha256((handoff / pin["path"]).read_bytes()).hexdigest() ==
                pin["sha256"], "Read-only source changed")
    print(json.dumps({
        "passed": True,
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
        "assignment_commit": contract["assignment_commit"],
        "image_sha256": proof["image"]["sha256"],
        "archival_source_pins": len(proof["sources"]),
        "byte_anchors_and_fresh_gnu_decodes": len(seen),
        "constants": 1,
        "original_execution_or_emulation": False,
        "scope": "Local bytes and decoding; not whole-CFG reachability or original equivalence."
    }, indent=2))


if __name__ == "__main__":
    main()
