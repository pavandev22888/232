"""Verify W8 context initialization, raw32 relations and the new handler frontier, statically."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from zipfile import ZipFile


def require(value, message):
    if not value:
        raise ValueError(message)


def normalized(instruction):
    return re.sub(r"\s+", "", instruction.split("#")[0]).lower()


def checked_file(handoff, pin):
    path = (handoff / pin["path"]).resolve(strict=True)
    require(path.is_relative_to(handoff), "Source outside handoff")
    data = path.read_bytes()
    require(len(data) == pin["size"], "Size: " + pin["path"])
    require(hashlib.sha256(data).hexdigest() == pin["sha256"], "Hash: " + pin["path"])
    return path


def main():
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    handoff = parser.parse_args().handoff.resolve(strict=True)
    proof = json.loads((here / "anchors.json").read_text())
    archive = checked_file(handoff, proof["archive"])
    for source in proof["sources"]:
        checked_file(handoff, source)
    with ZipFile(archive) as bundle:
        raw = bundle.read(proof["archive"]["member"])
    image = proof["image"]
    require(len(raw) == image["size"], "Image member size")
    require(hashlib.sha256(raw).hexdigest() == image["sha256"], "Image member hash")
    restored = handoff / image["path"]
    if restored.exists():
        require(checked_file(handoff, image).read_bytes() == raw, "Restored/archive image differ")
    base = int(image["base"], 16)
    scratch = here.parents[3] / "runs/w8-aggregate/static"
    require(not scratch.resolve().is_relative_to(handoff), "Scratch inside handoff")
    scratch.mkdir(parents=True, exist_ok=True)
    checked = set()
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        window = Path(temporary) / "window.bin"
        for start, end in proof["spans"]:
            start, end = int(start, 16), int(end, 16)
            require(base <= start < end <= base + len(raw), "Span bounds")
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
            expected = [(int(row["va"], 16), row["bytes"], normalized(row["instruction"]))
                        for row in proof["instructions"] if start <= int(row["va"], 16) < end]
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
        require(0 <= offset <= len(raw) - len(data), "Constant bounds")
        require(raw[offset:offset + len(data)] == data, "Constant bytes")
    print(json.dumps({"passed": True, "instructions": len(checked),
                      "spans": len(proof["spans"]), "constants": len(proof["constants"]),
                      "image_sha256": image["sha256"], "raw_image_source": "pinned input ZIP",
                      "original_execution_loading_emulation": False,
                      "scope": "Context initialization, observed raw32 relations and pinned handler frontier; not runtime providers"},
                     indent=2))


if __name__ == "__main__":
    main()
