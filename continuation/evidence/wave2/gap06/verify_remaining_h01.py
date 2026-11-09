#!/usr/bin/env python3
"""Classify/recheck 47 non-instruction H01 entries without executing DLL code."""

import argparse
from collections import Counter
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASE = 0x412A0000
KINDS = ("freshly_rechecked", "model_only", "historical_only", "unresolved")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_pe_helper(path):
    # Inspected and hash-pinned utility; never call mapped() or run legacy verifiers.
    spec = importlib.util.spec_from_file_location("h01_read_only_pe", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.PE


def resource_read(raw, va, size):
    offset = va - BASE
    require(offset >= 0 and offset + size <= len(raw), "resource read outside file")
    return raw[offset:offset + size]


def djb2(text):
    value = 5381
    for byte in text.encode("ascii"):
        value = (33 * value + byte) & 0xFFFFFFFF
    return value


def rol32(value, count):
    count &= 31
    value &= 0xFFFFFFFF
    return ((value << count) | (value >> ((32 - count) & 31))) & 0xFFFFFFFF


def reload_model():
    value = (0x9D8E6158 + 0x52A66FE7 - 0x2EB6FD3F) & 0xFFFFFFFF
    value = (rol32(value, 3) - 0x0B893606) & 0xFFFFFFFF
    result = rol32(value, 23)
    require(result == 0x32B5, "reload literal model differs")
    fixtures = [0, 1, 0x12345678, 0xFFFFFFFF]
    require(all(rol32(x, 32) == x for x in fixtures), "32-bit count masking differs")
    return {"result": hex(result), "rotate_by_32_fixtures": len(fixtures),
            "scope": "Authored modular arithmetic only; no instruction execution."}


def bias_model(raw):
    mask = resource_read(raw, 0x4205B050, 16)
    bias_bytes = resource_read(raw, 0x4205A9B0, 8)
    require(mask == bytes.fromhex("0000000000003043") * 2, "unexpected bias mask")
    bias = struct.unpack("<d", bias_bytes)[0]
    require(bias == 2**52, "unexpected bias constant")
    values = list(range(1024)) + [2**31 - 1, 2**32 - 1]
    for number in values:
        bits = struct.unpack("<Q", mask[:8])[0] | number
        value = struct.unpack("<d", struct.pack("<Q", bits))[0] - bias
        require(value == float(number), f"bias model differs for {number}")
    return {"mask_va": "0x4205b050", "mask_bytes": mask.hex(),
            "bias_va": "0x4205a9b0", "bias_bytes": bias_bytes.hex(),
            "cases": len(values), "domain": "0..1023 and two unsigned boundaries",
            "scope": "Authored IEEE-754 model, not x87 rounding or live-state proof."}


def executable(client, va):
    return any(s["flags"] & 0x20000000 and
               0 <= va - client.base - s["rva"] < s["raw_size"]
               for s in client.sections)


def decode_all(evidence, raw, client, outer, scratch):
    objdump = shutil.which("objdump")
    require(objdump is not None, "GNU objdump is required")
    version = subprocess.run([objdump, "--version"], capture_output=True, text=True,
                             check=True, timeout=10).stdout.splitlines()[0]
    rows = []
    with tempfile.TemporaryDirectory(prefix="decode-", dir=scratch) as temporary:
        window = Path(temporary) / "window.bin"
        for anchor in evidence:
            va = int(anchor["va"], 16)
            expected = bytes.fromhex(anchor["bytes"])
            offset = int(anchor["file_offset"], 16)
            if anchor["source"] == "resource":
                data = resource_read(raw, va, 96)
                file_data = outer
            else:
                require(anchor["source"] == "client", "unknown instruction source")
                require(client.file_offset(va - client.base) == offset,
                        f"client VA/file-offset mismatch {va:#x}")
                data = client.read_va(va, 96)
                file_data = client.data
            require(data[:len(expected)] == expected and
                    file_data[offset:offset + len(expected)] == expected,
                    f"anchor bytes differ at {va:#x}")
            window.write_bytes(data)
            output = subprocess.run(
                [objdump, "-D", "-b", "binary", "-m", "i386", "-Mintel",
                 "--insn-width=16", "--adjust-vma=" + hex(va), str(window)],
                capture_output=True, text=True, check=True, timeout=10).stdout
            match = re.search(r"^\s*" + f"{va:x}" +
                              r":\s*((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$", output, re.M)
            require(match is not None, f"GNU emitted no instruction at {va:#x}")
            decoded = bytes.fromhex(match[1])
            require(decoded == expected, f"GNU instruction bytes/length differ {va:#x}")
            rows.append({"va": hex(va), "source": anchor["source"],
                         "bytes": decoded.hex(), "length": len(decoded),
                         "instruction": match[2]})
    return {"tool": version, "records": len(rows),
            "sources": dict(Counter(r["source"] for r in rows)), "decodes": rows}


def evaluate(item, client, raw, images, evidence, scratch):
    ordinal, name, expected = item["ordinal"], item["check"], item["expected_historical_detail"]
    if ordinal in (1, 2, 3, 4):
        path = ("input/dll/" + name.split()[-1]) if ordinal <= 3 else "work/nl_handoff_2026-11-06/nl.bin"
        actual = digest(images[path])
        require(actual == expected, f"image hash differs: {path}")
        return {"path": path, "sha256": actual}
    if 169 <= ordinal <= 172:
        text = name.removeprefix("Full client string and DJB2 hash ")
        pattern = b"\0" + text.encode("ascii") + b"\0"
        offsets = [match.start() + 1 for match in re.finditer(re.escape(pattern), client.data)]
        require(offsets, f"no standalone full string: {text}")
        actual = hex(djb2(text))
        require(actual == expected, f"DJB2 differs: {text}")
        return {"string": text, "file_offsets": [hex(o) for o in offsets], "djb2": actual}
    if 173 <= ordinal <= 176:
        va = int(name.split()[-1], 16)
        actual = client.ascii(va)
        require(actual == expected, f"client string differs at {va:#x}")
        return {"va": hex(va), "file_offset": hex(client.file_offset(va-client.base)), "string": actual}
    if ordinal == 177:
        va = 0x10DA317C
        data = client.read_va(va, 16)
        field_type, pointer, offset, count, flags = struct.unpack("<IIIHH", data)
        require((field_type, client.ascii(pointer), offset, count) ==
                (6, "m_bInReload", 0x32B5, 1), "reload descriptor differs")
        return {"va": hex(va), "bytes": data.hex(), "field_type": field_type,
                "name_pointer": hex(pointer), "name": client.ascii(pointer),
                "offset": hex(offset), "count": count, "flags": flags}
    if ordinal == 178:
        literal_anchors = {a["va"]: a for a in evidence}
        for va in [0x41B71CFF, 0x41B71D04, 0x41B71D0C, 0x41B71D11,
                   0x41B71D17, 0x41B71D1C, 0x41B71D09, 0x41B71D14]:
            anchor = literal_anchors[hex(va)]
            require(resource_read(raw, va, len(bytes.fromhex(anchor["bytes"]))) ==
                    bytes.fromhex(anchor["bytes"]), "reload model anchor differs")
        return reload_model()
    if ordinal == 179:
        va = 0x10DB9E60
        data = client.read_va(va, 8)
        pointer, offset = struct.unpack("<II", data)
        require((client.ascii(pointer), offset) == ("m_fAccuracyPenalty", 0x3340),
                "accuracy descriptor differs")
        return {"va": hex(va), "bytes": data.hex(), "name_pointer": hex(pointer),
                "name": client.ascii(pointer), "offset": hex(offset)}
    if 180 <= ordinal <= 191:
        vtable = int(name.split()[-1], 16)
        kind = (ordinal - 180) % 3
        if kind == 0:
            locator = client.u32(vtable - 4)
            signature, offset, cd_offset, descriptor, hierarchy = struct.unpack(
                "<5I", client.read_va(locator, 20))
            actual = client.ascii(descriptor + 8)
            require(signature == 0 and offset == 0 and cd_offset == 0 and actual == expected,
                    f"primary RTTI differs at {vtable:#x}")
            return {"vtable": hex(vtable), "locator": hex(locator),
                    "type_descriptor": hex(descriptor), "class": actual, "primary_offset": offset,
                    "class_hierarchy": hex(hierarchy)}
        if kind == 1:
            pointers = [client.u32(vtable + slot * 4) for slot in range(484)]
            require(all(executable(client, pointer) for pointer in pointers),
                    f"non-executable method pointer at {vtable:#x}")
            return {"vtable": hex(vtable), "slots_checked": 484, "last_slot": 483,
                    "pointer_bytes_sha256": digest(client.read_va(vtable, 484 * 4)),
                    "scope": "File-backed executable section membership, not live dispatch."}
        target = client.u32(vtable + 0x78C)
        require(target == int(expected, 16), f"virtual+78C differs at {vtable:#x}")
        return {"vtable": hex(vtable), "slot": 483, "target": hex(target)}
    if 192 <= ordinal <= 197:
        va = int(name.split()[-1], 16)
        data = resource_read(raw, va, 4)
        actual = struct.unpack("<f", data)[0]
        require(actual == expected, f"float constant differs at {va:#x}")
        return {"va": hex(va), "bytes": data.hex(), "float32": actual}
    if ordinal == 198:
        return bias_model(raw)
    if ordinal == 201:
        return decode_all(evidence, raw, client, images["input/dll/neverlose.dll"], scratch)
    raise ValueError(f"no supported evaluator for check {ordinal}")


def recheck_entry(item, evaluator, resource_fact):
    row = {key: value for key, value in item.items() if key != "expected_historical_detail"}
    row["fresh_evaluation_passed"] = None
    if item["disposition"] in ("freshly_rechecked", "model_only"):
        try:
            row["new_evidence"] = evaluator()
            row["fresh_evaluation_passed"] = True
        except (ValueError, OSError, struct.error, subprocess.SubprocessError) as error:
            row["fresh_evaluation_passed"] = False
            row["error"] = str(error)
    elif item["ordinal"] in (5, 6):
        row["corroboration_not_named_tool_replay"] = resource_fact
    return row


def verify(handoff, scratch):
    manifest_path = HERE / "dispositions.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    workspace = handoff.resolve(strict=True) / "workspace"
    hashes = {}
    for relative, expected in (manifest["source_hashes"] | manifest["input_hashes"]).items():
        path = workspace / relative
        require(path.resolve(strict=True).is_relative_to(workspace.resolve()), "source escapes workspace")
        actual = digest(path.read_bytes())
        require(actual == expected, f"pinned source hash differs: {relative}")
        hashes[relative] = actual
    metrics = json.loads((workspace / manifest["metrics_path"]).read_text(encoding="utf-8"))
    actual_entries = [(i, c["check"]) for i, c in enumerate(metrics["checks"], 1)
                      if not c["check"].startswith("Original instruction ")]
    require(actual_entries == [(c["ordinal"], c["check"]) for c in manifest["checks"]] and
            len(actual_entries) == 47 and len(metrics["checks"]) == 209,
            "historical H01 partition differs")
    with (workspace / "orchestration/v1/direct/reproducibility.csv").open(
            encoding="utf-8-sig", newline="") as stream:
        ledger = next(row for row in csv.DictReader(stream) if row["id"] == "H01")
    require(ledger["script_present"] == "False" and ledger["strict_old_protocol_repeated"] == "False",
            "reassess original protocol availability: pinned ledger differs")
    PE = load_pe_helper(workspace / "orchestration/v7/pe_data.py")
    images = {path: (workspace / path).read_bytes() for path in manifest["input_hashes"]}
    raw = images["work/nl_handoff_2026-11-06/nl.bin"]
    client = PE(workspace / "input/dll/client.dll")
    outer = PE(workspace / "input/dll/neverlose.dll")
    resources = [row for row in outer.resources() if row["path"] == ("BINARY", 101, 1049)]
    require(len(resources) == 1, "expected one BINARY/101/lang1049 resource")
    resource = resources[0]
    require((resource["file_offset"], resource["size"]) == (0x629B10, 0x3501000),
            "resource extent differs")
    extracted = outer.data[resource["file_offset"]:resource["file_offset"] + resource["size"]]
    require(extracted == raw, "resource content differs from nl.bin")
    resource_fact = {"path": list(resource["path"]), "file_offset": hex(resource["file_offset"]),
                     "size": resource["size"], "sha256": digest(extracted),
                     "method": "hash-pinned read-only PE helper; not pefile/LIEF"}
    evidence = json.loads((workspace / "orchestration/v7/historical_H01_byte_evidence.json").read_text())
    require(len(evidence) == 162 and
            Counter(a["source"] for a in evidence) == {"resource": 143, "client": 19},
            "instruction subset differs")
    scratch.mkdir(parents=True, exist_ok=True)
    results = []
    for item in manifest["checks"]:
        require(item["disposition"] in KINDS, "invalid disposition")
        results.append(recheck_entry(
            item, lambda: evaluate(item, client, raw, images, evidence, scratch), resource_fact))
    after = {path: digest((workspace / path).read_bytes()) for path in manifest["input_hashes"]}
    unchanged = after == manifest["input_hashes"]
    return {"schema": 1, "supported_checks_passed": unchanged and
            all(r["fresh_evaluation_passed"] is not False for r in results),
            "baseline_commit": manifest["baseline_commit"],
            "verifier_sha256": digest(Path(__file__).read_bytes()),
            "manifest_sha256": digest(manifest_path.read_bytes()),
            "strict_old_protocol_repeated": False, "all_209_repeated": False,
            "external_independent_audit": False, "original_execution_or_emulation": False,
            "scope": "47 non-instruction H01 entries; archived pass flags/counts are not success inputs.",
            "disposition_counts": {kind: sum(r["disposition"] == kind for r in results) for kind in KINDS},
            "new_evaluation_passes": dict(Counter(r["disposition"] for r in results
                                                 if r["fresh_evaluation_passed"] is True)),
            "input_hashes_before": manifest["input_hashes"], "input_hashes_after": after,
            "inputs_unchanged_during_this_replay_only": unchanged,
            "pinned_sources": hashes, "supplemental_resource_fact": resource_fact,
            "unresolved_gates": ["Original producer script and exact tool/block-input protocol are not available via the cited paths/ledger; the archived check list is not an executable protocol.",
                                 "Named pefile/LIEF, Rizin and five VEX checks are not rerun.",
                                 "The original audit before/after interval cannot be recreated.",
                                 "Models do not establish live behavior, full equivalence or actual x87 rounding."],
            "checks": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify(args.handoff, ROOT / "runs/wave2/gap06")
    except (ValueError, OSError, KeyError, struct.error) as error:
        print(json.dumps({"supported_checks_passed": False, "strict_old_protocol_repeated": False,
                          "error": str(error)}, ensure_ascii=False, indent=2))
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["supported_checks_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
