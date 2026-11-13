"""Regression for false positive UART evidence and map verification."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.test import serial_handoff_ok, serial_memory_ok


def row(label, start, end):
    return f'STYLEOS MEMMAP {label} 0x{start:016x} 0x{end:016x}\r\n'.encode()


class SerialEvidenceTests(unittest.TestCase):
    x86 = b'STYLEOS x86_64 ExitBootServices OK\nSTYLEOS HANDOFF PASS x86_64\n'
    arm = (b'STYLEOS AARCH64: initial EL1\nSTYLEOS AARCH64: current EL1\n'
           b'STYLEOS HANDOFF PASS aarch64\n')
    halt = b'STYLEOS KERNEL HALT\r\n'
    diag = b'STYLEOS MEMMAP DIAGNOSTIC ONLY\r\n'
    passed = b'STYLEOS MEMMAP PASS\r\n'

    def test_normal_requires_handoff_and_halt(self):
        self.assertTrue(serial_handoff_ok(self.x86, 'x86_64'))
        self.assertFalse(serial_handoff_ok(self.halt, 'x86_64'))
        self.assertFalse(serial_handoff_ok(b'STYLEOS HANDOFF PASS x86_64', 'x86_64'))
        self.assertFalse(serial_handoff_ok(self.x86, 'aarch64'))
        self.assertFalse(serial_memory_ok(self.x86 + self.halt, 'x86_64', 'normal'))

    def test_prior_failure_cannot_be_hidden_by_later_success(self):
        for error in (b'STYLEOS BOOT_ERROR invalid UEFI system table\n',
                      b'STYLEOS AARCH64 BOOT ERROR: invalid DTB header\n',
                      b'STYLEOS BOOT_INFO ERROR: version\n',
                      b'early exception\n'):
            with self.subTest(error=error):
                self.assertFalse(serial_handoff_ok(error + self.x86, 'x86_64'))

    def test_negative_requires_exact_expected_error_without_success(self):
        expected = b'STYLEOS BOOT_INFO ERROR: magic\r\n'
        self.assertTrue(serial_memory_ok(expected, 'x86_64', 'invalid'))
        for invalid in (b'', b'BOOT_ERROR\n' + expected, expected + self.x86,
                        expected + self.passed, expected.replace(b'magic', b'magics')):
            self.assertFalse(serial_memory_ok(invalid, 'x86_64', 'invalid'))

    def test_arm_must_prove_requested_initial_and_final_el(self):
        self.assertFalse(serial_handoff_ok(self.arm, 'aarch64', el2=True))
        self.assertTrue(serial_handoff_ok(self.arm, 'aarch64'))
        el2 = self.arm.replace(b'initial EL1', b'initial EL2')
        self.assertTrue(serial_handoff_ok(el2, 'aarch64', el2=True))
        self.assertFalse(serial_handoff_ok(el2.replace(b'current EL1', b'current EL2'),
                                          'aarch64', el2=True))

    def test_memory_negative_checks_exact_reason_and_no_partial_map(self):
        for arch, good, reason in [('x86_64', self.x86, b'uefi-format'),
                                   ('aarch64', self.arm, b'fdt-format')]:
            error = b'STYLEOS MEMMAP ERROR: ' + reason + b'\r\n'
            self.assertTrue(serial_memory_ok(good + error, arch, 'invalid-map'))
            for bad in (error, good + error + self.halt, good + error + self.passed,
                        good + self.diag + error, good + error.replace(reason, b'range')):
                self.assertFalse(serial_memory_ok(bad, arch, 'invalid-map'))

    def test_normal_rejects_wrong_candidates_and_missing_live_reservations(self):
        ram = row('RAM', 0x1000, 0x40000)
        image = row('IMAGE', 0x2100, 0x15500)
        stack = row('STACK', 0x3000, 0x13000)
        info = row('HANDOFF', 0x14500, 0x14560)
        source = row('SOURCE', 0x1a900, 0x1b022)
        reservation1, reservation2 = row('RESERVED', 0x2000, 0x16000), row('RESERVED', 0x1a000, 0x1c000)
        prefix = self.x86 + self.diag + image + stack + info + source + ram + reservation1 + reservation2
        suffix = self.passed + self.halt
        candidates = [row('CANDIDATE', 0x1000, 0x2000), row('CANDIDATE', 0x16000, 0x1a000),
                      row('CANDIDATE', 0x1c000, 0x40000)]
        good = prefix + b''.join(candidates) + suffix
        self.assertTrue(serial_memory_ok(good, 'x86_64', 'normal'))
        for bad in (good.replace(reservation2, b''), good.replace(stack, b''),
                    good.replace(candidates[1], row('CANDIDATE', 0x15000, 0x1a000)),
                    good.replace(candidates[0], b''), good.replace(candidates[2], candidates[2] * 2),
                    good.replace(candidates[2], row('CANDIDATE', 0x1c000, 0x41000)),
                    good.replace(suffix, self.halt + self.passed)):
            self.assertFalse(serial_memory_ok(bad, 'x86_64', 'normal'))


if __name__ == '__main__':
    unittest.main()
