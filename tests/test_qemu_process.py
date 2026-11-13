"""Isolated runner lifecycle regressions; fake processes are not boot evidence.

Only the UART classifier is mocked. The real subprocess, pipes, bounded settling
window and intentional termination path in Checks.run_qemu() are exercised.
Tiny dummy image bytes have matching metadata hashes; no QEMU/OVMF is required.
"""
from pathlib import Path
import hashlib
import json
import os
import signal
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.test import Checks


@unittest.skipUnless(os.name == 'posix', 'runner uses POSIX pipes and signals')
class QemuProcessLifecycleTests(unittest.TestCase):
    marker = b'STYLEOS EMERGENCY HALT\n'

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='styleos-fake-qemu-')
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        target = self.folder / 'aarch64'
        target.mkdir()
        binary, image = b'unit-test dummy ELF', b'unit-test dummy raw Image'
        (target / 'kernel.elf').write_bytes(binary)
        (target / 'styleos.bin').write_bytes(image)
        (target / 'fault-site.json').write_text(json.dumps({
            'arch': 'aarch64',
            'symbol': 'styleos_test_fault',
            'binary_sha256': hashlib.sha256(binary).hexdigest(),
            'raw_image_sha256': hashlib.sha256(image).hexdigest(),
        }))
        self.runner = Checks(self.folder)

    def run_fake(self, after_marker, *, before_marker='', marker=None, timeout=5):
        fake = self.folder / 'fake-qemu'
        payload = self.marker if marker is None else marker
        fake.write_text(
            f'#!{sys.executable}\n'
            'import os, signal, time\n'
            'signal.alarm(10)\n'  # Independent failsafe for a broken harness.
            + before_marker + '\n'
            + f'os.write(1, {payload!r})\n'
            + after_marker + '\n')
        fake.chmod(0o700)
        job = ('unit-fake-qemu', 'aarch64', str(fake), self.folder, None,
               'exception', False, 128, timeout)
        with patch('tools.test.serial_exception_ok', return_value=True) as classify:
            result = self.runner.run_qemu(job)
        return result, classify

    def assert_natural_exit_rejected(self, result, classify, code):
        self.assertEqual(result[1], 'FAIL', result)
        self.assertIn('exited/reset', result[2])
        self.assertIn('natural_exit=True', result[3])
        self.assertIn('requested_stop=False', result[3])
        self.assertIn(f'returncode={code}', result[3])
        classify.assert_not_called()

    def test_success_marker_does_not_hide_natural_exit_zero(self):
        result, classify = self.run_fake('os._exit(0)')
        self.assert_natural_exit_rejected(result, classify, 0)

    def test_success_marker_does_not_hide_natural_exit_one(self):
        result, classify = self.run_fake('os._exit(1)')
        self.assert_natural_exit_rejected(result, classify, 1)

    def test_reset_after_marker_during_observation_is_rejected(self):
        result, classify = self.run_fake('time.sleep(0.05)\nos._exit(0)')
        self.assert_natural_exit_rejected(result, classify, 0)

    def test_process_that_stays_alive_is_stopped_by_harness(self):
        result, classify = self.run_fake('while True:\n    time.sleep(1)')
        self.assertEqual(result[1], 'PASS', result)
        self.assertIn('requested_stop=True', result[3])
        self.assertIn('natural_exit=False', result[3])
        self.assertIn('settled=True', result[3])
        self.assertIn(f'returncode={-signal.SIGTERM}', result[3])
        classify.assert_called_once()
        self.assertEqual(bytes(classify.call_args.args[0]), self.marker)

    def test_graceful_sigterm_exit_zero_is_accepted(self):
        result, classify = self.run_fake(
            'while True:\n    time.sleep(1)',
            before_marker='signal.signal(signal.SIGTERM, lambda *_: os._exit(0))')
        self.assertEqual(result[1], 'PASS', result)
        self.assertIn('requested_stop=True', result[3])
        self.assertIn('natural_exit=False', result[3])
        self.assertIn('returncode=0', result[3])
        classify.assert_called_once()

    def test_unrelated_exit_status_after_requested_stop_is_rejected(self):
        result, classify = self.run_fake(
            'while True:\n    time.sleep(1)',
            before_marker='signal.signal(signal.SIGTERM, lambda *_: os._exit(1))')
        self.assertEqual(result[1], 'FAIL', result)
        self.assertIn('unexpected QEMU process exit status', result[2])
        self.assertIn('requested_stop=True', result[3])
        self.assertIn('returncode=1', result[3])
        classify.assert_not_called()

    def test_unterminated_marker_is_not_complete_terminal_evidence(self):
        result, classify = self.run_fake('while True:\n    time.sleep(1)',
                                        marker=self.marker.rstrip(b'\n'), timeout=0.5)
        self.assertEqual(result[1], 'FAIL', result)
        self.assertIn('settled=False', result[3])
        classify.assert_not_called()

    def test_timeout_before_stable_terminal_evidence_is_not_green(self):
        # On a slow host the deadline can expire before Python emits its line;
        # either missing or not-yet-settled evidence must remain FAIL.
        result, classify = self.run_fake('while True:\n    time.sleep(1)', timeout=0.1)
        self.assertEqual(result[1], 'FAIL', result)
        self.assertIn('settled=False', result[3])
        classify.assert_not_called()


if __name__ == '__main__':
    unittest.main()
