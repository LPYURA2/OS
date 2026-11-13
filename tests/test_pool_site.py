"""Full interval proofs from independently constructed ELF/PE byte fixtures."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pool_site import describe_pool, POOL_SYMBOL, METADATA_SYMBOL

ARM_BASE = 0x40080000
PE_BASE = 0x140000000
POOL_BYTES = 0x8000
META_BYTES = 0x1000
ELF_PH = 64
ELF_SYM = 0x200
ELF_SH = 0x800
PE_OPT = 0x98
PE_SH = PE_OPT + 112


def arm_fixture():
    data = bytearray(0x1040)
    raw = bytearray(64)
    raw[:4] = bytes.fromhex('10000014')
    struct.pack_into('<QQQ', raw, 8, 0x80000, 0xa000, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM_BASE,
                     ELF_PH, ELF_SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH, 1, 5, 0x1000,
                     ARM_BASE, ARM_BASE, 64, 64, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH + 56, 1, 6, 0x1040,
                     ARM_BASE + 0x1000, ARM_BASE + 0x1000, 0, 0x9000, 0x1000)
    data[0x1000:0x1040] = raw
    strings = b'\0' + b'\0'.join(n.encode() for n in
        (POOL_SYMBOL, METADATA_SYMBOL, 'image_start', '__image_end', '__bss_start', '__bss_end')) + b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[0x400:0x400 + len(strings)] = strings
    data[0x600:0x600 + len(names)] = names
    symbols = [(POOL_SYMBOL, 0x11, 2, ARM_BASE + 0x1000, POOL_BYTES),
               (METADATA_SYMBOL, 0x11, 2, ARM_BASE + 0x9000, META_BYTES),
               ('image_start', 0x10, 1, ARM_BASE, 0),
               ('__image_end', 0x10, 2, ARM_BASE + 0xa000, 0),
               ('__bss_start', 0x10, 2, ARM_BASE + 0x1000, 0),
               ('__bss_end', 0x10, 2, ARM_BASE + 0xa000, 0)]
    for n, (name, info, section, value, size) in enumerate(symbols, 1):
        struct.pack_into('<IBBHQQ', data, ELF_SYM + 24 * n,
                         strings.index(name.encode()), info, 0, section, value, size)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (names.index(b'.text'), 1, 6, ARM_BASE, 0x1000, 64, 0, 0, 8, 0),
                (names.index(b'.bss'), 8, 3, ARM_BASE + 0x1000, 0x1040, 0x9000, 0, 0, 0x1000, 0),
                (names.index(b'.symtab'), 2, 0, 0, ELF_SYM, 7 * 24, 4, 1, 8, 24),
                (names.index(b'.strtab'), 3, 0, 0, 0x400, len(strings), 0, 0, 1, 0),
                (names.index(b'.shstrtab'), 3, 0, 0, 0x600, len(names), 0, 0, 1, 0)]
    for n, row in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, ELF_SH + n * 64, *row)
    return data, raw


def pe_fixture(raw_bytes=0, base=PE_BASE):
    data = bytearray(0x200 + raw_bytes)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 1, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PE_OPT, 0x20b)
    struct.pack_into('<Q', data, PE_OPT + 24, base)
    struct.pack_into('<II', data, PE_OPT + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, PE_OPT + 56, 0xa000, 0x200)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH, b'.data\0\0\0',
                     0x9000, 0x1000, raw_bytes, 0x200 if raw_bytes else 0,
                     0, 0, 0, 0, 0xc0000040 if raw_bytes else 0xc0000080)
    return data


def map_fixture(base=PE_BASE, pool=0x1000, meta=0x9000):
    return (f'fixture\n Preferred load address is {base:016x}\n\n'
            ' Address Publics by Value Rva+Base Lib:Object\n\n'
            f' 0001:{pool - 0x1000:08x} {POOL_SYMBOL} {base + pool:016x} fixture.obj\n'
            f' 0001:{meta - 0x1000:08x} {METADATA_SYMBOL} {base + meta:016x} fixture.obj\n')


class PoolSiteTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='styleos-pool-site-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def arm(self, data=None, raw=None):
        defaults = arm_fixture()
        data = defaults[0] if data is None else data
        raw = defaults[1] if raw is None else raw
        elf_path, raw_path = self.folder / 'kernel.elf', self.folder / 'styleos.bin'
        elf_path.write_bytes(data)
        raw_path.write_bytes(raw)
        return describe_pool('aarch64', elf_path, raw_path=raw_path)

    def pe(self, data=None, text=None):
        data = pe_fixture() if data is None else data
        text = map_fixture() if text is None else text
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(data)
        map_path.write_text(text)
        return describe_pool('x86_64', binary, map_path)

    def test_arm_full_nobits_extent_and_raw_image_size(self):
        result = self.arm()
        self.assertEqual((result['pool_start'], result['pool_end']),
                         (ARM_BASE + 0x1000, ARM_BASE + 0x9000))
        self.assertEqual((result['metadata_start'], result['metadata_end']),
                         (ARM_BASE + 0x9000, ARM_BASE + 0xa000))
        self.assertEqual(result['metadata_end'], result['image_end'])
        self.assertEqual(result['metadata_end'], result['bss_end'])
        # The raw file intentionally omits the 36-KiB zero-filled interval.
        self.assertEqual(result['image_size'], 0xa000)
        self.assertEqual(result['binary_sha256'], hashlib.sha256(arm_fixture()[0]).hexdigest())
        self.assertEqual(result['raw_sha256'], hashlib.sha256(arm_fixture()[1]).hexdigest())

    def test_pe_rvas_include_last_byte_and_ignore_preferred_runtime_address(self):
        result = self.pe()
        self.assertEqual(result['pool_rva_start'], 0x1000)
        self.assertEqual(result['pool_rva_end'], 0x9000)
        self.assertEqual(result['metadata_rva_end'], result['image_size'])
        self.assertEqual(result['preferred_image_base'], PE_BASE)
        self.assertEqual(result['address_space'], 'image-rva')
        self.assertNotIn('runtime_base', result)
        self.assertEqual(result['map_sha256'], hashlib.sha256(map_fixture().encode()).hexdigest())

    def test_pe_zero_preferred_base_and_initialized_zero_prefix(self):
        result = self.pe(pe_fixture(0x200, 0), map_fixture(0))
        self.assertEqual(result['preferred_image_base'], 0)
        self.assertEqual(result['pool_rva_start'], 0x1000)

    def test_missing_required_sidecar_and_invalid_arch(self):
        path = self.folder / 'binary'
        path.write_bytes(b'')
        for arch in ('aarch64', 'x86_64', 'm32'):
            with self.subTest(arch=arch), self.assertRaises(ValueError):
                describe_pool(arch, path)
        with self.assertRaises(ValueError):
            describe_pool('aarch64', self.folder, raw_path=path)

    def test_arm_rejects_incomplete_interval_proofs(self):
        mutations = [
            ('pool-size', ELF_SYM + 24 + 16, '<Q', POOL_BYTES - 1),
            ('meta-size', ELF_SYM + 48 + 16, '<Q', META_BYTES + 1),
            ('pool-start-alignment', ELF_SYM + 24 + 8, '<Q', ARM_BASE + 0x1001),
            ('meta-overlap', ELF_SYM + 48 + 8, '<Q', ARM_BASE + 0x8000),
            ('section-end-cross', ELF_SH + 2 * 64 + 32, '<Q', 0x8fff),
            ('load-end-cross', ELF_PH + 56 + 40, '<Q', 0x8fff),
            ('load-read-only', ELF_PH + 56 + 4, '<I', 4),
            ('load-physical-mismatch', ELF_PH + 56 + 24, '<Q', ARM_BASE + 0x2000),
            ('bss-clear-end-cross', ELF_SYM + 6 * 24 + 8, '<Q', ARM_BASE + 0x9000),
            ('image-end-cross', ELF_SYM + 4 * 24 + 8, '<Q', ARM_BASE + 0x9000),
            ('not-nobits', ELF_SH + 2 * 64 + 4, '<I', 1),
            ('not-write', ELF_SH + 2 * 64 + 8, '<Q', 2),
            ('not-alloc', ELF_SH + 2 * 64 + 8, '<Q', 1),
            ('not-object', ELF_SYM + 24 + 4, '<B', 0x10),
            ('not-global', ELF_SYM + 24 + 4, '<B', 1),
            ('undefined', ELF_SYM + 24 + 6, '<H', 0),
            ('overflow', ELF_SYM + 24 + 8, '<Q', (1 << 64) - 0x1000),
        ]
        for label, offset, fmt, value in mutations:
            with self.subTest(case=label), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.arm(data)

    def test_arm_missing_duplicate_and_truncated_symbols(self):
        missing, _ = arm_fixture()
        struct.pack_into('<I', missing, ELF_SYM + 24, 0)
        duplicate, _ = arm_fixture()
        duplicate[ELF_SYM + 7 * 24:ELF_SYM + 8 * 24] = duplicate[ELF_SYM + 24:ELF_SYM + 48]
        struct.pack_into('<Q', duplicate, ELF_SH + 3 * 64 + 32, 8 * 24)
        for data in (missing, duplicate, b'', arm_fixture()[0][:0x103f]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                self.arm(data)

    def test_arm_raw_header_and_initialized_bytes_must_match(self):
        for offset, fmt, value in ((8, '<Q', 0), (16, '<Q', 0x9000), (56, '<I', 0)):
            with self.subTest(header_offset=offset), self.assertRaises(ValueError):
                data, raw = arm_fixture()
                # Alter both headers: matching headers alone are not proof.
                struct.pack_into(fmt, raw, offset, value)
                struct.pack_into(fmt, data, 0x1000 + offset, value)
                self.arm(data, raw)
        with self.assertRaises(ValueError):
            self.arm(raw=arm_fixture()[1][:63])
        data, raw = arm_fixture()
        raw[0] ^= 1
        with self.assertRaises(ValueError):
            self.arm(data, raw)

    def test_pe_rejects_full_interval_end_crossing_and_bad_layout(self):
        mutations = [
            ('section-end-cross', PE_SH + 8, '<I', 0x8fff),
            ('image-end-cross', PE_OPT + 56, '<I', 0x9fff),
            ('section-readonly', PE_SH + 36, '<I', 0x40000080),
            ('section-code-only', PE_SH + 36, '<I', 0xe0000020),
            ('wrong-machine', 0x84, '<H', 0xaa64),
            ('PE32', PE_OPT, '<H', 0x10b),
            ('preferred-overflow', PE_OPT + 24, '<Q', (1 << 64) - 0x1000),
        ]
        for label, offset, fmt, value in mutations:
            with self.subTest(case=label), self.assertRaises(ValueError):
                data = pe_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.pe(data)
        for text in (map_fixture(meta=0x8000), map_fixture(pool=0x1001),
                     map_fixture(meta=0x9001), map_fixture(pool=0xa000)):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.pe(text=text)

    def test_pe_rejects_nonzero_initialization_and_truncated_raw(self):
        data = pe_fixture(0x200)
        data[0x200] = 1
        with self.assertRaises(ValueError):
            self.pe(data)
        with self.assertRaises(ValueError):
            self.pe(pe_fixture(0x200)[:-1])

    def test_pe_rejects_missing_duplicate_or_inconsistent_map(self):
        text = map_fixture()
        row = text.splitlines()[-1]
        cases = ['', text.replace(POOL_SYMBOL, 'another'), text + row + '\n',
                 text.replace('Rva+Base', 'RuntimeBase'), text + '\0',
                 text.replace('0001:00008000', '0002:00008000'),
                 text.replace('0001:00008000', '0001:00008001'),
                 text.replace('Preferred load address is 0000000140000000',
                              'Preferred load address is 0000000180000000'),
                 map_fixture(pool=1 << 32),
                 text.replace('0000000140001000', '000000013fffffff')]
        for n, case in enumerate(cases):
            with self.subTest(case=n), self.assertRaises(ValueError):
                self.pe(text=case)

    def test_arm_rejects_partially_overlapping_load(self):
        data, _ = arm_fixture()
        struct.pack_into('<H', data, 56, 3)
        struct.pack_into('<IIQQQQQQ', data, ELF_PH + 2 * 56, 1, 4, 0x1040,
                         ARM_BASE + 0x8fff, ARM_BASE + 0x8fff, 0, 1, 1)
        with self.assertRaises(ValueError):
            self.arm(data)

    def test_pe_rejects_partially_overlapping_section(self):
        data = pe_fixture()
        struct.pack_into('<H', data, 0x86, 2)
        struct.pack_into('<8sIIIIIIHHI', data, PE_SH + 40, b'.alias',
                         1, 0x8fff, 0, 0, 0, 0, 0, 0, 0x40000080)
        with self.assertRaises(ValueError):
            self.pe(data)

    def test_pe_rejects_ambiguous_overlapping_sections(self):
        data = pe_fixture()
        struct.pack_into('<H', data, 0x86, 2)
        data[PE_SH + 40:PE_SH + 80] = data[PE_SH:PE_SH + 40]
        with self.assertRaises(ValueError):
            self.pe(data)


if __name__ == '__main__':
    unittest.main()
