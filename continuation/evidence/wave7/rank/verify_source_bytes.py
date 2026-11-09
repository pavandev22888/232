"""Check read-only image pins, complete local windows, fresh i386 decodes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def require(value, message):
    if not value:
        raise ValueError(message)


def normalized(instruction):
    return re.sub(r"\s+", "", instruction.split("#")[0]).lower()


def main():
    here = Path(__file__).resolve().parent
    repo = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    handoff = args.handoff.resolve(strict=True)
    scratch = repo / "runs/w7-rank-static"
    require(not scratch.resolve().is_relative_to(handoff), "Scratch inside handoff")
    scratch.mkdir(parents=True, exist_ok=True)
    proof = json.loads((here / "anchors.json").read_text())
    for pin in [proof["image"], *proof["sources"]]:
        path = (handoff / pin["path"]).resolve(strict=True)
        require(path.is_relative_to(handoff), "Source outside handoff")
        data = path.read_bytes()
        require(len(data) == pin["size"], "Size: " + pin["path"])
        require(hashlib.sha256(data).hexdigest() == pin["sha256"], "Hash: " + pin["path"])
    raw = (handoff / proof["image"]["path"]).read_bytes()
    base = int(proof["image"]["base"], 16)
    checked = set()
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        window = Path(directory) / "window.bin"
        for start, end in proof["spans"]:
            start, end = int(start, 16), int(end, 16)
            window.write_bytes(raw[start - base:end - base])
            output = subprocess.check_output([
                "objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
                "--insn-width=16", "--adjust-vma=" + hex(start), str(window)], text=True)
            decoded = []
            for line in output.splitlines():
                match = re.match(r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)\s*(.*)$", line)
                if match:
                    decoded.append((int(match[1], 16), "".join(match[2].split()),
                                    normalized(match[3])))
            rows = [r for r in proof["instructions"] if start <= int(r["va"], 16) < end]
            expected = [(int(r["va"], 16), r["bytes"], normalized(r["instruction"])) for r in rows]
            require(decoded == expected, "Fresh decode mismatch at " + hex(start))
            cursor = start
            for va, data, _ in decoded:
                require(va == cursor and va not in checked, "Window gap/duplicate")
                require(raw[va - base:va - base + len(data) // 2].hex() == data, "Bytes")
                cursor += len(data) // 2
                checked.add(va)
            require(cursor == end, "Incomplete window")
    require(len(checked) == len(proof["instructions"]), "Unverified instructions")
    for constant in proof["constants"]:
        offset = int(constant["va"], 16) - base
        data = bytes.fromhex(constant["bytes"])
        require(raw[offset:offset + len(data)] == data, "Constant bytes")
    print(json.dumps({"passed": True, "instructions": len(checked),
                      "image_sha256": proof["image"]["sha256"],
                      "original_execution_loading_emulation": False,
                      "scope": "Local ranking tail and constants, not runtime providers or full selector"}, indent=2))


if __name__ == "__main__":
    main()
