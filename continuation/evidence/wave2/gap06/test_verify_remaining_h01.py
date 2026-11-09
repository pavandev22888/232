#!/usr/bin/env python3
"""Focused safety/epistemic boundary checks for the bounded H01 verifier."""

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("remaining_h01", HERE / "verify_remaining_h01.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class RemainingH01Tests(unittest.TestCase):
    def test_manifest_partition(self):
        manifest = json.loads((HERE / "dispositions.json").read_text())
        checks = manifest["checks"]
        self.assertEqual(len({r["ordinal"] for r in checks}), 47)
        self.assertEqual(dict(verifier.Counter(r["disposition"] for r in checks)),
                         {"freshly_rechecked": 33, "model_only": 2, "historical_only": 12})

    def test_archived_pass_cannot_become_fresh_success(self):
        callback = Mock(return_value={"passed": True})
        row = verifier.recheck_entry({"ordinal": 202, "disposition": "historical_only",
                                      "archived_pass": True}, callback, {})
        callback.assert_not_called()
        self.assertIsNone(row["fresh_evaluation_passed"])

    def test_new_failure_is_not_promoted(self):
        row = verifier.recheck_entry({"ordinal": 1, "disposition": "freshly_rechecked"},
                                      Mock(side_effect=ValueError("wrong bytes")), {})
        self.assertIs(row["fresh_evaluation_passed"], False)
        self.assertEqual(row["error"], "wrong bytes")

    def test_changed_pinned_source_is_rejected(self):
        scratch = verifier.ROOT / "runs/wave2/gap06"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="pin-rejection-", dir=scratch) as directory:
            handoff = Path(directory)
            path = handoff / "workspace/orchestration/v7/verify_v7.py"
            path.parent.mkdir(parents=True)
            path.write_text("changed source; deliberately not executable")
            with self.assertRaisesRegex(ValueError, "pinned source hash differs"):
                verifier.verify(handoff, scratch)

    def test_modular_model_and_count_masking(self):
        self.assertEqual(verifier.reload_model()["result"], "0x32b5")
        self.assertEqual(verifier.rol32(0x12345678, 32), 0x12345678)
        self.assertEqual(verifier.rol32(0x80000000, 1), 1)

    def test_bias_model_is_bounded_and_checks_constant(self):
        mask = bytes.fromhex("0000000000003043") * 2
        with patch.object(verifier, "resource_read", side_effect=[mask, mask[:8]]):
            self.assertEqual(verifier.bias_model(b"")["cases"], 1026)
        with patch.object(verifier, "resource_read", side_effect=[bytes(16), bytes(8)]):
            with self.assertRaisesRegex(ValueError, "bias mask"):
                verifier.bias_model(b"")

    def test_only_file_backed_executable_targets(self):
        client = SimpleNamespace(base=0x1000, sections=[
            {"rva": 0x200, "raw_size": 0x10, "virtual_size": 0x20, "flags": 0x20000000},
            {"rva": 0x300, "raw_size": 0x10, "flags": 0}])
        self.assertTrue(verifier.executable(client, 0x120F))
        self.assertFalse(verifier.executable(client, 0x1210))
        self.assertFalse(verifier.executable(client, 0x1300))

    def test_descriptor_must_match_actual_bytes(self):
        client = Mock()
        client.read_va.return_value = struct.pack("<IIIHH", 6, 0x1234, 0x32B6, 1, 2)
        client.ascii.return_value = "m_bInReload"
        with self.assertRaisesRegex(ValueError, "reload descriptor differs"):
            verifier.evaluate({"ordinal": 177, "check": "Reload descriptor type/name/offset/size",
                               "expected_historical_detail": None}, client, b"", {}, [], HERE)


if __name__ == "__main__":
    unittest.main()
