"""Exception evidence must prove the right fault, PC, ordering and terminal state."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.test import serial_exception_ok, serial_handoff_ok, serial_idt_ready


def row(kind, start, end):
    return f'STYLEOS MEMMAP {kind} 0x{start:016x} 0x{end:016x}\r\n'.encode()


def prefix(arch, el2=False):
    if arch == 'x86_64':
        out = (b'STYLEOS x86_64 ExitBootServices OK\n'
               b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        base = 0x60000000
        ram = [(base, base + 0x80000)]
        reserved = [(base, base + 0x20000), (base + 0x22000, base + 0x23000)]
        candidates = [(base + 0x20000, base + 0x22000), (base + 0x23000, base + 0x80000)]
        source = reserved[1]
    else:
        out = (b'STYLEOS AARCH64: initial EL2\n' if el2 else b'STYLEOS AARCH64: initial EL1\n')
        out += b'STYLEOS AARCH64: current EL1\n'
        base = 0x40080000
        ram = [(0x40000000, 0x48000000)]
        source = (0x44000000, 0x44001000)
        reserved = [(0x09000000, 0x09001000), (base, base + 0x20000), source]
        candidates = [(0x40000000, base), (base + 0x20000, source[0]),
                      (source[1], 0x48000000)]
    out += (b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n' +
            f'STYLEOS HANDOFF PASS {arch}\n'.encode() + b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, ranges in [('IMAGE', [(base, base + 0x20000)]),
                         ('STACK', [(base + 0x8000, base + 0x18000)]),
                         ('HANDOFF', [(base + 0x19000, base + 0x19060)]),
                         ('SOURCE', [source]), ('RAM', ram),
                         ('RESERVED', reserved), ('CANDIDATE', candidates)]:
        for start, end in ranges:
            out += row(kind, start, end)
    return out + b'STYLEOS MEMMAP PASS\n'


class ExceptionOutputTests(unittest.TestCase):
    x86_site = {'arch': 'x86_64', 'symbol': 'styleos_test_fault',
                'rva': 0x1234, 'preferred_image_base': 0x140000000,
                'instruction_hex': '0f0b'}
    arm_site = {'arch': 'aarch64', 'symbol': 'styleos_test_fault',
                'pc': 0x40081000, 'image_base': 0x40080000,
                'instruction_hex': '002420d4'}
    emergency = b'STYLEOS EMERGENCY HALT\n'
    arm_record = (b'STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x3c reason=brk '
                  b'pc=0x0000000040081000 esr=0x00000000f2000120 '
                  b'elr=0x0000000040081000 spsr=0x00000000000003c5 far=absent\n')
    x86_record = (b'STYLEOS EXCEPTION arch=x86_64 kind=sync vector=6 reason=invalid-opcode '
                  b'pc=0x0000000060001234 error=absent cr2=absent\n')
    x86_probe = b'STYLEOS EXCEPTION TEST x86_64 pc=0x0000000060001234\n'

    def test_x86_uses_actual_loaded_base_not_preferred_base(self):
        good = prefix('x86_64') + self.x86_probe + self.x86_record + self.emergency
        self.assertTrue(serial_exception_ok(good, 'x86_64', self.x86_site))
        wrong = good.replace(b'0x0000000060001234', b'0x0000000140001234')
        self.assertFalse(serial_exception_ok(wrong, 'x86_64', self.x86_site))
        self.assertFalse(serial_exception_ok(good, 'x86_64', dict(self.x86_site, rva=0x1235)))

    def test_arm_brk_and_initial_el2_are_distinct_evidence(self):
        for el2 in (False, True):
            good = prefix('aarch64', el2) + self.arm_record + self.emergency
            self.assertTrue(serial_exception_ok(good, 'aarch64', self.arm_site, el2=el2))
            self.assertFalse(serial_exception_ok(good, 'aarch64', self.arm_site, el2=not el2))

    def test_missing_duplicate_or_normal_halt_is_not_success(self):
        base = prefix('aarch64')
        good = base + self.arm_record + self.emergency
        for bad in (base, base + self.emergency, base + self.arm_record,
                    good + b'STYLEOS KERNEL HALT\n', good + self.arm_record,
                    good + self.emergency, self.arm_record + self.emergency + base):
            self.assertFalse(serial_exception_ok(bad, 'aarch64', self.arm_site))

    def test_wrong_class_saved_pc_or_invented_far_is_rejected(self):
        good = prefix('aarch64') + self.arm_record + self.emergency
        for old, new in [(b'class=0x3c', b'class=0x25'), (b'reason=brk', b'reason=alignment-fault'),
                         (b'esr=0x00000000f2000120', b'esr=0x0000000096000061'),
                         (b'elr=0x0000000040081000', b'elr=0x0000000040081004'),
                         (b'far=absent', b'far=0x0000000040081000'),
                         (b'spsr=0x00000000000003c5', b'spsr=0x00000000000003c9')]:
            self.assertFalse(serial_exception_ok(good.replace(old, new), 'aarch64', self.arm_site))

    def test_ud_must_not_publish_padding_or_cr2_as_hardware_fields(self):
        good = prefix('x86_64') + self.x86_probe + self.x86_record + self.emergency
        for old, new in [(b'vector=6', b'vector=14'), (b'error=absent', b'error=0x0000000000000000'),
                         (b'cr2=absent', b'cr2=0x0000000000000000')]:
            self.assertFalse(serial_exception_ok(good.replace(old, new), 'x86_64', self.x86_site))

    def test_all_old_modes_reject_unexpected_emergency_records(self):
        for marker in [self.arm_record, self.x86_record, self.emergency]:
            self.assertFalse(serial_handoff_ok(prefix('x86_64') + marker, 'x86_64'))
            self.assertFalse(serial_handoff_ok(b'STYLEOS BOOT_INFO ERROR: magic\n' + marker,
                                              'x86_64', invalid=True))

    def test_idt_marker_follows_ebs_on_the_way_to_common_entry(self):
        good = prefix('x86_64')
        self.assertTrue(serial_idt_ready(good))
        for bad in (good.replace(b'cs=0x0000000000000038', b'cs=0x000000000000003b'),
                    good.replace(b'STYLEOS x86_64 ExitBootServices OK', b'no EBS'),
                    good.replace(b'STYLEOS kernel_main entered', b'no entry')):
            self.assertFalse(serial_idt_ready(bad))


if __name__ == '__main__':
    unittest.main()
