"""Test static evidence rejection, not original-image behavior."""

import unittest

from verify_preservation import check_local_structure, read_table


class PreservationEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.rows = read_table()

    def replace(self, address, instruction):
        for row in self.rows:
            if row[0] == address:
                row[2] = instruction
                return
        self.fail(f"missing fixture address {address}")

    def test_supported_local_structure(self):
        check_local_structure(self.rows)

    def test_missing_restore_is_rejected(self):
        self.replace("0x41c5c712", "pop eax")
        with self.assertRaisesRegex(ValueError, "ESI/EBP writes"):
            check_local_structure(self.rows)

    def test_additional_esi_mutation_is_rejected(self):
        self.replace("0x41c5bdb7", "mov esi,ecx")
        with self.assertRaisesRegex(ValueError, "ESI/EBP writes"):
            check_local_structure(self.rows)

    def test_changed_frame_extent_is_rejected(self):
        self.replace("0x41c5bdbf", "mov dword ptr [esi+0xb0],0xffffffff")
        with self.assertRaisesRegex(ValueError, "store extent"):
            check_local_structure(self.rows)

    def test_unestablished_branch_target_is_rejected(self):
        self.replace("0x41c5c24e", "je 0x41c5c730")
        with self.assertRaisesRegex(ValueError, "supported local branch"):
            check_local_structure(self.rows)


if __name__ == "__main__":
    unittest.main()
