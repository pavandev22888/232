"""Read original bytes; decode as data. Never load or execute an original image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def normalized(instruction):
    return re.sub(r"\s+", " ", instruction.split("#")[0]).strip().lower()


def check(condition, message):
    if not condition:
        raise ValueError(message)


def verify(handoff, continuation):
    proof = json.loads((continuation / "evidence/byte_anchors.json").read_text())
    workspace = handoff / "workspace"
    raw = (workspace / proof["image"]["path"]).read_bytes()
    check(hashlib.sha256(raw).hexdigest() == proof["image"]["sha256"],
          "Original image SHA256 differs")
    base = int(proof["image_base"], 16)
    walks = {}
    build = continuation / "build"
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="byte_check_", dir=build) as directory:
        window = Path(directory) / "instructions.bin"
        for row in proof["instructions"]:
            address = int(row["va"], 16)
            expected = bytes.fromhex(row["bytes"])
            offset = address - base
            check(0 <= offset <= len(raw) - len(expected), row["va"] + ": range")
            check(raw[offset:offset + len(expected)] == expected, row["va"] + ": bytes")
            if row["source"] not in walks:
                walk = json.loads((workspace / row["source"]).read_text())
                walks[row["source"]] = {
                    (item["va"], item["bytes"], normalized(item["instruction"]))
                    for item in walk["instructions"]
                }
            check((row["va"], row["bytes"], normalized(row["instruction"])) in
                  walks[row["source"]], row["va"] + ": archived instruction missing")
            window.write_bytes(raw[offset:offset + 64])
            decoded = subprocess.run(
                ["objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
                 "--insn-width=16", f"--adjust-vma={address:#x}", str(window)],
                capture_output=True, text=True, check=True, timeout=10).stdout
            match = re.search(r"^\s*" + f"{address:x}" +
                              r":\s+((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", decoded, re.M)
            check(match is not None, row["va"] + ": decode absent")
            check("".join(match[1].split()) == row["bytes"], row["va"] + ": decode bytes")
            check(normalized(match[2]) == normalized(row["instruction"]),
                  row["va"] + ": decode instruction")
    for constant in proof["constants"]:
        offset = int(constant["address"], 16) - base
        expected = bytes.fromhex(constant["bytes"])
        check(raw[offset:offset + len(expected)] == expected, constant["meaning"])
    inventory = json.loads((continuation / "evidence/inventory.json").read_text())
    for item in inventory["files"]:
        data = (workspace / item["path"]).read_bytes()
        check(len(data) == item["size"] and hashlib.sha256(data).hexdigest() == item["sha256"],
              "Original source changed: " + item["path"])
    result = {
        "passed": True,
        "image_sha256": proof["image"]["sha256"],
        "instruction_bytes_and_fresh_decodes": len(proof["instructions"]),
        "constants": len(proof["constants"]),
        "unchanged_inventory_files": len(inventory["files"]),
        "scope": "Bytes and instruction decoding only; not branch reachability or C++ equivalence.",
    }
    (build / "evidence_check.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path,
                        default=root.parent / "runs/handoff/RAGEBOT_HANDOFF_V10")
    args = parser.parse_args()
    verify(args.handoff.resolve(), root)
