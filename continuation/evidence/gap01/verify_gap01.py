#!/usr/bin/env python3
"""Read-only byte and provenance checks for the bounded GAP01 findings."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path


EXPECTED_SHA256 = "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
BASE = 0x412A0000

ROWS = [
    ("orchestration/v3/point_allocate_walk.json", "0x41478448"),
    ("orchestration/v3/point_allocate_walk.json", "0x41478456"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb7a"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb7f"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb8c"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb8f"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb92"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bb96"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bbbf"),
    ("orchestration/v5/target_builder_walk.json", "0x4149bbc2"),
    ("orchestration/v5/target_constructor_walk.json", "0x4148b3c7"),
    ("orchestration/v5/target_constructor_walk.json", "0x4148b48e"),
    ("orchestration/v7/target_population_walk.json", "0x414a4f42"),
    ("orchestration/v7/target_population_walk.json", "0x414a4fa9"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147a2a9"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147a2af"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147f648"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147f652"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147f668"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147f66e"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147f674"),
    ("orchestration/v7/support_target_scan_walk.json", "0x414a8dbb"),
    ("orchestration/v7/support_selector_walk.json", "0x41477e83"),
    ("orchestration/v7/support_selector_walk.json", "0x41477e89"),
    ("orchestration/v7/support_selector_walk.json", "0x41477e90"),
    ("orchestration/v7/support_selector_walk.json", "0x41477e96"),
    ("orchestration/v7/support_selector_walk.json", "0x41477e9c"),
    ("orchestration/v7/support_selector_walk.json", "0x41477ea3"),
    ("orchestration/v5/command_preparation_walk.json", "0x41595497"),
    ("orchestration/v5/historical_reset_block_walk.json", "0x415decee"),
    ("orchestration/v5/stop_complete_walk.json", "0x4148cbf0"),
    ("orchestration/v5/support_target_coordinator_walk.json", "0x4147adae"),
]


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def norm_instruction(text):
    return " ".join(text.lower().split())


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    repo = Path(__file__).resolve().parents[3]
    parser.add_argument(
        "--handoff",
        type=Path,
        default=repo / "runs/handoff/RAGEBOT_HANDOFF_V10",
    )
    args = parser.parse_args()

    handoff = args.handoff.resolve()
    workspace = handoff / "workspace"
    image = workspace / "work/nl_handoff_2026-11-06/nl.bin"
    raw = image.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    require(digest == EXPECTED_SHA256, f"unexpected image SHA-256: {digest}")
    require(shutil.which("objdump") is not None, "GNU objdump is required")

    checked = []
    for source, va_text in ROWS:
        va = int(va_text, 16)
        walk_path = workspace / source
        walk = load_json(walk_path)
        row = next(
            (item for item in walk["instructions"] if item["va"].lower() == va_text),
            None,
        )
        require(row is not None, f"missing saved instruction: {source} {va_text}")
        expected = bytes.fromhex(row["bytes"])
        offset = va - BASE
        require(offset >= 0 and raw[offset : offset + len(expected)] == expected,
                f"image bytes differ at {va_text} from {source}")

        disassembly = subprocess.run(
            [
                "objdump", "-D", "-b", "binary", "-m", "i386", "-Mintel",
                f"--adjust-vma={BASE:#x}",
                f"--start-address={va:#x}",
                f"--stop-address={va + len(expected):#x}",
                str(image),
            ],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        decoded = None
        for line in disassembly.splitlines():
            match = re.match(rf"^\s*{va:x}:\s*.*?\t(.+?)\s*$", line, re.I)
            if match:
                decoded = match.group(1)
                break
        require(decoded is not None, f"objdump did not decode {va_text}")
        require(
            norm_instruction(decoded) == norm_instruction(row["instruction"]),
            f"fresh decode differs at {va_text}: {decoded!r} != {row['instruction']!r}",
        )
        checked.append({"va": va_text, "bytes": row["bytes"], "source": source})

    v5 = workspace / "orchestration/v5"
    extended = load_json(v5 / "extended_field_stores_known.json")
    point_candidate = next((x for x in extended if x["va"] == "0x4149bb92"), None)
    require(point_candidate is not None, "known +25 store review is missing")
    require(point_candidate["object_identity_proven"] is False,
            "Point+26 candidate identity unexpectedly changed")
    require(point_candidate["guard"] == "The EBX<65 branch skips this block.",
            "Point+26 candidate guard changed")

    wide = load_json(workspace / "orchestration/v7/wide_member_candidates.json")
    point_rows = [x for x in wide["point26"] if x["row"]["va"] == "0x4149bb92"]
    require(any("orchestration/v5/target_builder_walk.json" in x["sources"]
                for x in point_rows), "Point+26 candidate source mapping changed")
    t_rows = [x for x in wide["targetF0F4"] if x["row"]["va"] == "0x4147f648"]
    require(any("orchestration/v5/support_target_coordinator_walk.json" in x["sources"]
                for x in t_rows), "T+F4 scratch-candidate source mapping changed")
    bc_rows = wide["globalBC"]
    require(len(bc_rows) == 2, "saved G425009BC direct-store census changed")
    require(all(x["row"]["bytes"] == "c605bc09504200"
                and x["row"]["instruction"] == "mov BYTE PTR ds:0x425009bc,0x0"
                for x in bc_rows), "saved G425009BC census includes a nonzero store")

    chains = load_json(v5 / "baseline_source_chains.json")
    transfer = next((x for x in chains if x.get("field") == "T+96"), None)
    require(transfer is not None and transfer["status"] == "propagation_verified_positive_origin_open",
            "Point+26 downstream-chain status changed")

    print(json.dumps({
        "passed": True,
        "image_sha256": digest,
        "saved_rows_rechecked": len(checked),
        "fresh_objdump_decodes": len(checked),
        "point26_candidate_identity": point_candidate["object_identity_proven"],
        "point26_guard": point_candidate["guard"],
        "g425009bc_known_direct_stores": len(bc_rows),
        "t96_origin_status": transfer["status"],
        "scope": "Selected bytes/provenance only; not full source absence, reachability, or runtime behavior.",
    }, indent=2))


if __name__ == "__main__":
    main()
