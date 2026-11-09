#!/usr/bin/env python3
"""Read-only CODE01 review of the pinned baseline; builds use a temporary directory."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

BASELINE = "ed9df79c028e63ca56fc191a226f5fe2f221df64"
BYTE_CHECKS = {
    "4150bead": "8939",
    "4150beb6": "0f8423060000",
    "4150c4da": "0f92c0",
    "4150c4df": "31c0",
    "414fd82d": "e80ee60000",
    "414fd841": "89c3",
    "414fd905": "84db",
    "414fd907": "750d",
    "414fd90c": "0f2e4548",
    "414fd910": "0f8318020000",
}
PROBE = r'''
#include "rage/position_lifecycle.hpp"
#include <cstdlib>
#include <iostream>
using namespace rage::offline;
int checks = 0;
void check(bool value) { ++checks; if (!value) std::exit(1); }
void scenario(bool source, float now, float marker, ReadStatus status,
              ActorTransition expected) {
    PositionStore store;
    auto& record = store.at(17);
    record.active = 1;
    record.updated_at = 10.0f;
    record.validity_marker = marker;
    record.position = {1.0f, 2.0f, 3.0f};
    record.actor_position = {4.0f, 5.0f, 6.0f};
    const auto read = read_resolved_record(
        store, 17, {source, true, 1, {true, false, 0}, now});
    check(read.status == status);
    check(read.record == &record);
    int owner = 0;
    Vec3 sink{7.0f, 8.0f, 9.0f};
    const bool normal_return_al = read.status == ReadStatus::usable;
    const auto result = update_actor_consumer(
        {&owner, read, sink},
        {normal_return_al, ActorGuardOutcome::passed, false, std::nullopt, nullptr});
    check(result.transition == expected);
    check(result.record == &record && result.actor_owner == &owner);
}
int main() {
    scenario(false, 10.0f, 2.5f, ReadStatus::source_unavailable,
             ActorTransition::copied_record_position);
    scenario(true, 10.0f, 0.0f, ReadStatus::usable,
             ActorTransition::copied_record_position);
    scenario(true, 14.0f, 0.0f, ReadStatus::expired,
             ActorTransition::marker_gate_rejected);
    scenario(true, 14.0f, 2.5f, ReadStatus::expired,
             ActorTransition::copied_record_position);
    std::cout << "coherent normal-reader integration: " << checks << " checks passed\n";
}
'''


def digest_file(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument("--handoff", type=Path)
    parser.add_argument("--run-tests", action="store_true")
    args = parser.parse_args()
    repo = args.repo.resolve()
    handoff = (args.handoff or repo / "runs/handoff/RAGEBOT_HANDOFF_V10").resolve()

    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(repo), *arguments])

    def blob(path):
        return git("show", f"{BASELINE}:{path}")

    paths = git("ls-tree", "-r", "--name-only", BASELINE, "--", "continuation").decode().splitlines()
    selected = [p for p in paths if p == "continuation/Makefile" or
                p.startswith(("continuation/include/", "continuation/src/")) or
                (p.startswith("continuation/tests/test_") and p.endswith(".cpp"))]
    files = [{"path": p, "size": len(blob(p)),
              "sha256": hashlib.sha256(blob(p)).hexdigest(),
              "lines": len(blob(p).splitlines())} for p in selected]
    assert len(selected) == 10
    fixtures = blob("continuation/tests/test_position_lifecycle.cpp").decode().splitlines()
    assert "read.status == ReadStatus::source_unavailable" in fixtures[55]
    assert "binding, inputs(true," in fixtures[63]
    assert "read.status == ReadStatus::usable" in fixtures[252]
    assert "inputs(false," in fixtures[257]

    inventory = json.loads(blob("continuation/evidence/inventory.json"))["files"]
    recovered = handoff / "workspace/work/recovered_nl/recovered_nl"
    actual = {str(p.relative_to(handoff / "workspace"))
              for folder in ("src", "sdk", "tests")
              for p in (recovered / folder).iterdir() if p.is_file()}
    assert actual == {item["path"] for item in inventory}
    for item in inventory:
        path = handoff / "workspace" / item["path"]
        assert path.stat().st_size == item["size"], path
        assert digest_file(path) == item["sha256"], path
        assert len(path.read_bytes().splitlines()) == item["lines"], path

    image = handoff / "workspace/work/nl_handoff_2026-11-06/nl.bin"
    image_sha = digest_file(image)
    assert image_sha == "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e"
    with image.open("rb") as stream:
        for address, hex_bytes in BYTE_CHECKS.items():
            expected = bytes.fromhex(hex_bytes)
            stream.seek(int(address, 16) - 0x412A0000)
            assert stream.read(len(expected)) == expected, address

    result = {"baseline": BASELINE, "baseline_files": files,
              "original_inventory_verified": inventory,
              "original_inventory_files": len(inventory),
              "image_sha256": image_sha, "selected_byte_checks": BYTE_CHECKS,
              "contradictory_fixture_pairs_confirmed": 2,
              "runs": [], "original_execution_loading_or_emulation": False}
    if args.run_tests:
        with tempfile.TemporaryDirectory(prefix="code01-baseline-") as temporary:
            root = Path(temporary)
            for path in selected:
                destination = root / Path(path).relative_to("continuation")
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(blob(path))

            def run(command, cwd=None):
                completed = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
                record = {"command": command, "exit_code": completed.returncode,
                          "stdout": completed.stdout, "stderr": completed.stderr}
                result["runs"].append(record)
                if completed.returncode:
                    print(json.dumps(result, ensure_ascii=False, indent=2))
                    raise SystemExit(completed.returncode)

            run(["make", "-C", str(root), "test", "sanitize", f"BUILD={root / 'build'}"])
            (root / "integration_probe.cpp").write_text(PROBE)
            sources = [str(root / "src" / name) for name in
                       ("position_records.cpp", "position_lifecycle.cpp")]
            flags = ["g++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Wpedantic",
                     "-Werror", "-fno-fast-math", "-ffp-contract=off", "-fno-omit-frame-pointer",
                     "-fsanitize=address,undefined", "-I", str(root / "include")]
            run(flags + sources + [str(root / "integration_probe.cpp"), "-o", str(root / "probe")])
            run([str(root / "probe")])
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
