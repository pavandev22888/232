"""Recheck only H01 ordinal 5's archived resource predicate with pinned pefile."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import sys


PARSER_SHA256 = "0d0eb68f0f169182613dc64b3ab50b20855508c1c1d2faafe8fccf23edb6a345"
INPUTS = {
    "input/dll/neverlose.dll": "c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c",
    "work/nl_handoff_2026-11-06/nl.bin": "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e",
}
EXPECTED = [["BINARY", 6462224, 55578624, 1049]]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    args = parser.parse_args()
    spec = importlib.util.find_spec("pefile")
    require(spec is not None and spec.origin is not None, "pefile 2024.8.26 is required")
    parser_hash = digest(Path(spec.origin).read_bytes())
    require(parser_hash == PARSER_SHA256, "pefile source hash differs")
    import pefile

    require(pefile.__version__ == "2024.8.26", "pefile version differs")
    workspace = args.handoff.resolve() / "workspace"
    images = {}
    for name, expected in INPUTS.items():
        images[name] = (workspace / name).read_bytes()
        require(digest(images[name]) == expected, f"input SHA-256 differs: {name}")
    outer = images["input/dll/neverlose.dll"]
    resource = images["work/nl_handoff_2026-11-06/nl.bin"]
    rows, anchors = [], []
    # Supplying bytes avoids even a file-backed mapping of the original image.
    with pefile.PE(data=outer, fast_load=True) as pe:
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]]
        )
        for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            if str(kind.name) != "BINARY":
                continue
            for identifier in kind.directory.entries:
                if identifier.id != 101:
                    continue
                for language in identifier.directory.entries:
                    entry = language.data.struct
                    offset = pe.get_offset_from_rva(entry.OffsetToData)
                    data = outer[offset:offset + entry.Size]
                    require(data == resource, "resource bytes differ from pinned nl.bin")
                    rows.append([str(kind.name), offset, entry.Size, language.id])
                    anchors.append({
                        "resource_id": identifier.id,
                        "data_rva": hex(entry.OffsetToData),
                        "resource_sha256": digest(data),
                        "records": [
                            {"role": role, "file_offset": hex(item.get_file_offset()),
                             "bytes": outer[item.get_file_offset():
                                            item.get_file_offset() + item.sizeof()].hex()}
                            for role, item in (
                                ("type", kind.struct), ("id", identifier.struct),
                                ("language", language.struct), ("data", entry)
                            )
                        ],
                    })
    require(rows == EXPECTED, f"H01 ordinal 5 predicate differs: {rows!r}")
    print(json.dumps({
        "schema": 1,
        "ordinal": 5,
        "check": "pefile resource BINARY/101 raw offset and size",
        "result": "fresh_named_library_leaf_pass",
        "producer": "W5 replay_pefile_leaf.py, not the absent original H01 producer",
        "python": platform.python_version(),
        "pefile": pefile.__version__,
        "parser_sha256": parser_hash,
        "replay_sha256": digest(Path(__file__).read_bytes()),
        "input_sha256": INPUTS,
        "actual": rows,
        "anchors": anchors,
        "old_producer_executed": False,
        "strict_old_protocol_repeated": False,
        "all_209_repeated": False,
        "external_independent_audit": False,
        "original_dll_execution": False,
        "original_dll_loading": False,
        "original_emulation": False,
        "scope": "One resource-directory predicate; no instruction trace or runtime equivalence.",
    }, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"pefile leaf failed: {error}", file=sys.stderr)
        sys.exit(2)
