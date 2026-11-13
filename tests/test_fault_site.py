"""Binary-derived managed fault evidence, using independent synthetic fixtures."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.fault_site import describe_fault


ARM_BASE = 0x40080000
PE_BASE = 0x140000000
ELF_SHOFF = 0x300
ELF_SYMOFF = 0x180
PE_OPTIONAL = 0x80 + 24
PE_SECTIONS = PE_OPTIONAL + 112


def arm_fixture():
    data = bytearray(ELF_SHOFF + 5 * 64)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM_BASE,
                     64, ELF_SHOFF, 0, 64, 56, 1, 64, 5, 4)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 0x100, ARM_BASE,
                     ARM_BASE, 0x40, 0x40, 0x100)
    data[0x110:0x114] = bytes.fromhex('002420d4')
    strings = b'\0image_start\0styleos_test_fault\0'
    names = b'\0.text\0.symtab\0.strtab\0.shstrtab\0'
    data[0x200:0x200 + len(strings)] = strings
    data[0x240:0x240 + len(names)] = names
    struct.pack_into('<IBBHQQ', data, ELF_SYMOFF + 24,
                     strings.index(b'image_start'), 0x10, 0, 1, ARM_BASE, 0)
    struct.pack_into('<IBBHQQ', data, ELF_SYMOFF + 48,
                     strings.index(b'styleos_test_fault'), 0x10, 0,
                     1, ARM_BASE + 16, 0)
    sections = [
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (names.index(b'.text'), 1, 6, ARM_BASE, 0x100, 0x40, 0, 0, 4, 0),
        (names.index(b'.symtab'), 2, 0, 0, ELF_SYMOFF, 72, 3, 1, 8, 24),
        (names.index(b'.strtab'), 3, 0, 0, 0x200, len(strings), 0, 0, 1, 0),
        (names.index(b'.shstrtab'), 3, 0, 0, 0x240, len(names), 0, 0, 1, 0),
    ]
    for index, row in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, ELF_SHOFF + index * 64, *row)
    return data


def pe_fixture():
    data = bytearray(0x400)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 1, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PE_OPTIONAL, 0x20b)
    struct.pack_into('<I', data, PE_OPTIONAL + 16, 0x1000)
    struct.pack_into('<Q', data, PE_OPTIONAL + 24, PE_BASE)
    struct.pack_into('<II', data, PE_OPTIONAL + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, PE_OPTIONAL + 56, 0x2000, 0x200)
    struct.pack_into('<H', data, PE_OPTIONAL + 68, 10)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SECTIONS, b'.text\0\0\0',
                     0x100, 0x1000, 0x200, 0x200, 0, 0, 0, 0, 0x60000020)
    data[0x210:0x212] = bytes.fromhex('0f0b')
    return data


def map_fixture(address=PE_BASE + 0x1010, position='0001:00000010'):
    return ('fixture\n\n  Preferred load address is 0000000140000000\n\n'
            '  Address         Publics by Value              Rva+Base               Lib:Object\n\n'
            f' {position}       styleos_test_fault          {address:016x}     f fixture.obj\n')


class FaultSiteTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='styleos-fault-site-')
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)

    def arm(self, data=None):
        binary = self.folder / 'kernel.elf'
        binary.write_bytes(arm_fixture() if data is None else data)
        return describe_fault('aarch64', binary)

    def pe(self, data=None, text=None):
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(pe_fixture() if data is None else data)
        map_path.write_text(map_fixture() if text is None else text)
        return describe_fault('x86_64', binary, map_path)

    def test_arm_resolves_symbol_segment_and_exact_instruction(self):
        result = self.arm()
        self.assertEqual(result['arch'], 'aarch64')
        self.assertEqual(result['symbol'], 'styleos_test_fault')
        self.assertEqual(result['pc'], ARM_BASE + 16)
        self.assertEqual(result['offset'], 0x110)
        self.assertEqual(result['image_base'], ARM_BASE)
        self.assertEqual(result['image_offset'], 16)
        self.assertEqual(result['instruction_hex'], '002420d4')
        self.assertEqual(result['instruction_word'], 0xd4202400)

    def test_pe_uses_preferred_image_base_to_prove_rva(self):
        result = self.pe()
        self.assertEqual(result['arch'], 'x86_64')
        self.assertEqual(result['symbol'], 'styleos_test_fault')
        self.assertEqual(result['rva'], 0x1010)
        self.assertEqual(result['preferred_image_base'], PE_BASE)
        self.assertEqual(result['offset'], 0x210)
        self.assertEqual(result['instruction_hex'], '0f0b')
        # Runtime relocation is deliberately outside this binary-only API.
        self.assertNotIn('pc', result)
        self.assertNotIn('runtime_base', result)

    def test_pe_zero_preferred_base_is_not_mistaken_for_runtime_base(self):
        data = pe_fixture()
        struct.pack_into('<Q', data, PE_OPTIONAL + 24, 0)
        text = map_fixture(0x1010).replace('Preferred load address is 0000000140000000',
                                         'Preferred load address is 0000000000000000')
        result = self.pe(data, text)
        self.assertEqual(result['preferred_image_base'], 0)
        self.assertEqual(result['rva'], 0x1010)

    def test_arm_rejects_wrong_or_truncated_elf(self):
        for label, location, fmt, value in [
            ('class', 4, '<B', 1), ('endianness', 5, '<B', 2),
            ('machine', 18, '<H', 62), ('type', 16, '<H', 3),
            ('header-size', 52, '<H', 63), ('section-table', 40, '<Q', (1 << 64) - 1),
            ('program-table', 32, '<Q', (1 << 64) - 1),
            ('section-string-index', 62, '<H', 9),
        ]:
            with self.subTest(case=label):
                data = arm_fixture()
                struct.pack_into(fmt, data, location, value)
                with self.assertRaises(ValueError):
                    self.arm(data)
        for length in (0, 8, 63, 119, len(arm_fixture()) - 1):
            with self.subTest(truncated=length), self.assertRaises(ValueError):
                self.arm(arm_fixture()[:length])

    def test_arm_rejects_missing_duplicate_and_undefined_symbol(self):
        missing = arm_fixture()
        struct.pack_into('<I', missing, ELF_SYMOFF + 48, 0)
        duplicate = arm_fixture()
        duplicate[ELF_SYMOFF + 72:ELF_SYMOFF + 96] = duplicate[ELF_SYMOFF + 48:ELF_SYMOFF + 72]
        struct.pack_into('<Q', duplicate, ELF_SHOFF + 2 * 64 + 32, 96)
        undefined = arm_fixture()
        struct.pack_into('<H', undefined, ELF_SYMOFF + 48 + 6, 0)
        for label, data in [('missing', missing), ('duplicate', duplicate),
                            ('undefined', undefined)]:
            with self.subTest(case=label), self.assertRaises(ValueError):
                self.arm(data)

    def test_arm_rejects_unbacked_or_nonexecuting_fault(self):
        mutations = [
            ('section-not-executable', ELF_SHOFF + 64 + 8, '<Q', 2),
            ('segment-not-executable', 64 + 4, '<I', 4),
            ('section-segment-disagreement', 64 + 8, '<Q', 0x104),
            ('physical-virtual-disagreement', 64 + 24, '<Q', ARM_BASE + 0x1000),
            ('not-file-backed', 64 + 32, '<Q', 17),
            ('outside-section', ELF_SYMOFF + 48 + 8, '<Q', ARM_BASE + 0x40),
            ('unaligned', ELF_SYMOFF + 48 + 8, '<Q', ARM_BASE + 17),
            ('wrong-image-base', ELF_SYMOFF + 24 + 8, '<Q', ARM_BASE + 4),
            ('declared-symbol-overflow', ELF_SYMOFF + 48 + 16, '<Q', 0x40),
            ('local-fault-symbol', ELF_SYMOFF + 48 + 4, '<B', 0),
            ('symbol-string-outside-table', ELF_SYMOFF + 48, '<I', 0xffffffff),
        ]
        for label, location, fmt, value in mutations:
            with self.subTest(case=label):
                data = arm_fixture()
                struct.pack_into(fmt, data, location, value)
                with self.assertRaises(ValueError):
                    self.arm(data)

    def test_arm_requires_the_specific_brk_immediate(self):
        for instruction in ('000020d4', '200020d4', '1f2003d5', '0f0b0000'):
            with self.subTest(instruction=instruction):
                data = arm_fixture()
                data[0x110:0x114] = bytes.fromhex(instruction)
                with self.assertRaises(ValueError):
                    self.arm(data)

    def test_pe_rejects_wrong_headers_and_bounds(self):
        mutations = [
            ('machine', 0x84, '<H', 0xaa64), ('32-bit', PE_OPTIONAL, '<H', 0x10b),
            ('truncated-opt', 0x94, '<H', 111), ('no-sections', 0x86, '<H', 0),
            ('bad-pe-offset', 0x3c, '<I', 0xfffffff0),
            ('raw-outside-file', PE_SECTIONS + 20, '<I', 0x400),
            ('raw-overlaps-headers', PE_SECTIONS + 20, '<I', 0x100),
            ('outside-image', PE_OPTIONAL + 56, '<I', 0x1000),
        ]
        for label, location, fmt, value in mutations:
            with self.subTest(case=label):
                data = pe_fixture()
                struct.pack_into(fmt, data, location, value)
                with self.assertRaises(ValueError):
                    self.pe(data)
        for length in (0, 63, 0x97, len(pe_fixture()) - 1):
            with self.subTest(truncated=length), self.assertRaises(ValueError):
                self.pe(pe_fixture()[:length])

    def test_pe_rejects_missing_duplicate_and_suspicious_map_rows(self):
        original = map_fixture()
        row = original.splitlines()[-1]
        bad_maps = [
            '', original.replace('styleos_test_fault', 'other_symbol'),
            original + row + '\n',
            original.replace('Rva+Base', 'RuntimeAddress'),
            original.replace('styleos_test_fault', 'styleos_test_fault+2'),
            original.replace('0000000140001010', '-000000140001010'),
            original.replace('0000000140001010', '000000013fffffff'),
            map_fixture(position='0002:00000010'),
            map_fixture(position='0001:00000011'),
            original.replace('Preferred load address is 0000000140000000',
                             'Preferred load address is 0000000180000000'),
            original + '\0',
        ]
        for number, text in enumerate(bad_maps):
            with self.subTest(map_case=number), self.assertRaises(ValueError):
                self.pe(text=text)

    def test_pe_rejects_nonexecuting_virtual_padding_and_wrong_opcode(self):
        data = pe_fixture()
        struct.pack_into('<I', data, PE_SECTIONS + 36, 0x40000040)
        with self.assertRaises(ValueError):
            self.pe(data)
        data = pe_fixture()
        struct.pack_into('<I', data, PE_SECTIONS + 8, 0x500)
        with self.assertRaises(ValueError):
            self.pe(data, map_fixture(PE_BASE + 0x1400, '0001:00000400'))
        data = pe_fixture()
        data[0x210:0x212] = b'\x90\x90'
        with self.assertRaises(ValueError):
            self.pe(data)

    def test_pe_rejects_ambiguous_overlapping_sections(self):
        data = pe_fixture()
        struct.pack_into('<H', data, 0x86, 2)
        data[PE_SECTIONS + 40:PE_SECTIONS + 80] = data[PE_SECTIONS:PE_SECTIONS + 40]
        with self.assertRaises(ValueError):
            self.pe(data)

    def test_api_requires_supported_arch_binary_and_x86_map(self):
        binary = self.folder / 'BOOTX64.EFI'
        binary.write_bytes(pe_fixture())
        for arch, path, map_path in [('rv64', binary, None),
                                     ('x86_64', binary, None),
                                     ('aarch64', self.folder / 'missing.elf', None)]:
            with self.subTest(arch=arch), self.assertRaises(ValueError):
                describe_fault(arch, path, map_path)


if __name__ == '__main__':
    unittest.main()
