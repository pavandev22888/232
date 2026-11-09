#!/usr/bin/env python3
"""Verify immutable charge-slice provenance and byte anchors."""

import hashlib
import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[3]
DEFAULT_WORKSPACE = ROOT / "runs/handoff/RAGEBOT_HANDOFF_V10/workspace"
EVIDENCE = pathlib.Path(__file__).with_name("charge_state_evidence.json")


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def main() -> int:
    workspace = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_WORKSPACE
    evidence = json.loads(EVIDENCE.read_text(encoding="utf-8"))

    for relpath, expected in evidence["source_sha256"].items():
        path = workspace / relpath
        if not path.is_file():
            fail(f"missing source: {relpath}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            fail(f"source hash mismatch: {relpath}")

    image_info = evidence["image"]
    image_path = workspace / image_info["path"]
    image = image_path.read_bytes()
    expected_rows: dict[str, dict[str, dict[str, str]]] = {}
    for anchor in evidence["machine_byte_assertions"]:
        by_source = expected_rows.setdefault(anchor["walk"], {})
        by_source[anchor["va"]] = anchor

    for walk_relpath, anchors in expected_rows.items():
        walk = json.loads((workspace / walk_relpath).read_text(encoding="utf-8"))
        rows = {item["va"]: item for item in walk["instructions"]}
        for va, anchor in anchors.items():
            row = rows.get(va)
            if row is None:
                fail(f"instruction absent from walk: {walk_relpath} {va}")
            if row["bytes"] != anchor["bytes"] or row["instruction"] != anchor["instruction"]:
                fail(f"walk assertion mismatch: {walk_relpath} {va}")
            offset = int(va, 16) - int(image_info["image_base"], 16)
            expected = bytes.fromhex(anchor["bytes"])
            if offset < 0 or image[offset:offset + len(expected)] != expected:
                fail(f"image bytes mismatch: {va} at file offset {offset:#x}")

    frontier = json.loads(
        (workspace / "orchestration/v8/branch_frontier.json").read_text(
            encoding="utf-8"
        )
    )
    frontier_rows = {
        (item["module"], item["instruction"]["va"]): item["instruction"]["bytes"]
        for item in frontier
    }
    for item in evidence["unresolved_frontier_sites"]:
        key = (item["module"], item["va"])
        if frontier_rows.get(key) != item["bytes"]:
            fail(f"frontier assertion mismatch: {key}")

    print(
        "PASS: "
        f"{len(evidence['source_sha256'])} source hashes, "
        f"{len(evidence['machine_byte_assertions'])} instruction-byte assertions, "
        f"{len(evidence['unresolved_frontier_sites'])} unresolved frontier sites"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
