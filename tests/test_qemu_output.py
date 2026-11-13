"""Regression for boot errors followed by misleading success markers."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.test import serial_handoff_ok


class SerialEvidenceTests(unittest.TestCase):
    success = b'STYLEOS HANDOFF PASS x86_64\r\nSTYLEOS KERNEL HALT\r\n'

    def test_normal_requires_handoff_and_halt(self):
        self.assertTrue(serial_handoff_ok(self.success, 'x86_64'))
        self.assertFalse(serial_handoff_ok(b'STYLEOS KERNEL HALT', 'x86_64'))
        self.assertFalse(serial_handoff_ok(b'STYLEOS HANDOFF PASS x86_64', 'x86_64'))
        self.assertFalse(serial_handoff_ok(self.success, 'aarch64'))

    def test_prior_failure_cannot_be_hidden_by_later_success(self):
        for error in (b'STYLEOS BOOT_ERROR invalid UEFI system table\n',
                      b'STYLEOS AARCH64 BOOT ERROR: invalid DTB header\n',
                      b'STYLEOS BOOT_INFO ERROR: version\n',
                      b'early exception\n'):
            with self.subTest(error=error):
                self.assertFalse(serial_handoff_ok(error + self.success, 'x86_64'))

    def test_negative_requires_exact_expected_error_without_success(self):
        expected = b'STYLEOS BOOT_INFO ERROR: magic\r\n'
        self.assertTrue(serial_handoff_ok(expected, 'x86_64', invalid=True))
        self.assertFalse(serial_handoff_ok(b'', 'x86_64', invalid=True))
        self.assertFalse(serial_handoff_ok(expected + self.success, 'x86_64', invalid=True))
        self.assertFalse(serial_handoff_ok(b'BOOT_ERROR\n' + expected,
                                          'x86_64', invalid=True))

    def test_arm_must_prove_requested_initial_el(self):
        arm = self.success.replace(b'x86_64', b'aarch64')
        self.assertFalse(serial_handoff_ok(arm, 'aarch64'))
        self.assertTrue(serial_handoff_ok(b'initial EL1\n' + arm, 'aarch64'))
        self.assertFalse(serial_handoff_ok(b'initial EL1\n' + arm,
                                          'aarch64', el2=True))
        self.assertTrue(serial_handoff_ok(b'initial EL2\n' + arm,
                                         'aarch64', el2=True))


if __name__ == '__main__':
    unittest.main()
