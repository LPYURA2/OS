"""Independent ELF/PE fixtures for complete paging-reserve and read-PC proof."""
from pathlib import Path
import hashlib
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.paging_site import describe_paging, INITIAL, OBJECTS, POOL, FAULT

ARM = 0x40080000
PREFERRED = 0x140000000
SHOFF = 0xa00
SYMOFF = 0x300
PEOPT = 0x98
PESH = PEOPT + 112


def arm_fixture(fault=False):
    image_size = 0x2c000
    text_size = 68 if fault else 64
    raw = bytearray(0x2000)
    raw[:4] = bytes.fromhex('10000014')
    struct.pack_into('<QQQ', raw, 8, 0x80000, image_size, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    if fault:
        raw[64:68] = bytes.fromhex('010040f9')
    raw[0x1000:0x2000] = INITIAL
    data = bytearray(0x3000)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM,
                     64, SHOFF, 0, 64, 56, 2, 64, 7, 6)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 0x1000,
                     ARM, ARM, text_size, text_size, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, 120, 1, 6, 0x2000,
                     ARM + 0x1000, ARM + 0x1000, 0x1000, image_size - 0x1000, 0x1000)
    data[0x1000:0x1000 + text_size] = raw[:text_size]
    data[0x2000:0x3000] = INITIAL
    symbols = [('styleos_paging_data', 0x11, 2, ARM + 0x1000, 0x1000),
               ('styleos_paging_tables', 0x11, 3, ARM + 0x2000, 0x20000),
               ('styleos_paging_control', 0x11, 3, ARM + 0x22000, 0x1000),
               ('styleos_bootpool_storage', 0x11, 3, ARM + 0x23000, 0x8000),
               ('styleos_bootpool_metadata', 0x11, 3, ARM + 0x2b000, 0x1000),
               ('image_start', 0x10, 1, ARM, 0),
               ('__image_end', 0x10, 3, ARM + image_size, 0),
               ('__bss_start', 0x10, 3, ARM + 0x2000, 0),
               ('__bss_end', 0x10, 3, ARM + image_size, 0)]
    if fault:
        symbols.append((FAULT, 0x12, 1, ARM + 64, 4))
    strings = b'\0' + b'\0'.join(row[0].encode() for row in symbols) + b'\0'
    data[0x600:0x600 + len(strings)] = strings
    for n, (name, info, section, value, size) in enumerate(symbols, 1):
        struct.pack_into('<IBBHQQ', data, SYMOFF + n * 24,
                         strings.index(name.encode()), info, 0, section, value, size)
    names = b'\0.text\0.data\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[0x800:0x800 + len(names)] = names
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (names.index(b'.text'), 1, 6, ARM, 0x1000, text_size, 0, 0, 8, 0),
                (names.index(b'.data'), 1, 3, ARM + 0x1000, 0x2000, 0x1000, 0, 0, 0x1000, 0),
                (names.index(b'.bss'), 8, 3, ARM + 0x2000, 0x3000, image_size - 0x2000, 0, 0, 0x1000, 0),
                (names.index(b'.symtab'), 2, 0, 0, SYMOFF, (len(symbols) + 1) * 24, 5, 1, 8, 24),
                (names.index(b'.strtab'), 3, 0, 0, 0x600, len(strings), 0, 0, 1, 0),
                (names.index(b'.shstrtab'), 3, 0, 0, 0x800, len(names), 0, 0, 1, 0)]
    for n, section in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, SHOFF + n * 64, *section)
    return data, raw


def pe_fixture(fault=False):
    data = bytearray(0x1400)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 2, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PEOPT, 0x20b)
    struct.pack_into('<Q', data, PEOPT + 24, PREFERRED)
    struct.pack_into('<II', data, PEOPT + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, PEOPT + 56, 0x2d000, 0x200)
    struct.pack_into('<8sIIIIIIHHI', data, PESH, b'.text\0\0\0',
                     0x1000, 0x1000, 0x200, 0x200, 0, 0, 0, 0, 0x60000020)
    struct.pack_into('<8sIIIIIIHHI', data, PESH + 40, b'.data\0\0\0',
                     0x2b000, 0x2000, 0x1000, 0x400, 0, 0, 0, 0, 0xc0000040)
    data[0x400:0x1400] = INITIAL
    if fault:
        data[0x220:0x223] = bytes.fromhex('488b01')
    rows = [('styleos_paging_data', 2, 0x2000),
            ('styleos_paging_tables', 2, 0x3000),
            ('styleos_paging_control', 2, 0x23000),
            ('styleos_bootpool_storage', 2, 0x24000),
            ('styleos_bootpool_metadata', 2, 0x2c000)]
    if fault:
        rows.append((FAULT, 1, 0x1020))
    text = f'fixture\n Preferred load address is {PREFERRED:016x}\n\n'
    text += ' Address Publics by Value Rva+Base Lib:Object\n\n'
    for name, segment, rva in rows:
        text += f' {segment:04x}:{rva - segment * 0x1000:08x} {name} {PREFERRED+rva:016x} fixture.obj\n'
    return data, text


class PagingSiteTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='styleos-paging-site-')
        self.addCleanup(temp.cleanup)
        self.folder = Path(temp.name)

    def arm(self, data=None, raw=None, fault=False):
        defaults = arm_fixture(fault)
        elf, image = self.folder / 'kernel.elf', self.folder / 'styleos.bin'
        elf.write_bytes(defaults[0] if data is None else data)
        image.write_bytes(defaults[1] if raw is None else raw)
        return describe_paging('aarch64', elf, raw_path=image, fault=fault)

    def pe(self, data=None, text=None, fault=False):
        defaults = pe_fixture(fault)
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(defaults[0] if data is None else data)
        map_path.write_text(defaults[1] if text is None else text)
        return describe_paging('x86_64', binary, map_path, fault=fault)

    def test_arm_complete_nobits_and_image_size(self):
        result = self.arm()
        self.assertEqual(result['objects']['tables']['end'] - result['objects']['tables']['start'], 131072)
        self.assertEqual(result['objects']['tables']['start'], ARM + 0x2000)
        self.assertEqual(result['image_size'], 0x2c000)
        self.assertEqual(result['bss_end'], result['image_end'])
        self.assertEqual(result['raw_sha256'], hashlib.sha256(arm_fixture()[1]).hexdigest())
        self.assertNotIn('fault_site', result)

    def test_pe_virtual_tail_and_rvas(self):
        result = self.pe()
        self.assertEqual(result['objects']['tables']['start'], 0x3000)
        self.assertEqual(result['objects']['tables']['end'], 0x23000)
        self.assertEqual(result['objects']['data']['start'], 0x2000)
        self.assertEqual(result['preferred_image_base'], PREFERRED)
        self.assertEqual(result['image_end'], 0x2d000)
        self.assertEqual(result['address_space'], 'image-rva')
        self.assertNotIn('runtime_base', result)

    def test_both_managed_read_sites(self):
        arm, pe = self.arm(fault=True), self.pe(fault=True)
        self.assertEqual(arm['fault_site']['pc'], ARM + 64)
        self.assertEqual(arm['fault_site']['image_offset'], 64)
        self.assertEqual(arm['fault_site']['instruction_hex'], '010040f9')
        self.assertEqual(pe['fault_site']['rva'], 0x1020)
        self.assertEqual(pe['fault_site']['instruction_hex'], '488b01')

    def test_fault_symbol_is_absent_from_positive_images(self):
        a, raw = arm_fixture(True)
        p, text = pe_fixture(True)
        with self.assertRaises(ValueError): self.arm(a, raw)
        with self.assertRaises(ValueError): self.pe(p, text)
        with self.assertRaises(ValueError): self.arm(fault=True, data=arm_fixture()[0], raw=arm_fixture()[1])
        with self.assertRaises(ValueError): self.pe(fault=True, data=pe_fixture()[0], text=pe_fixture()[1])

    def test_wrong_read_opcodes_rejected(self):
        a, raw = arm_fixture(True)
        a[0x1040] ^= 1; raw[64] ^= 1
        with self.assertRaises(ValueError): self.arm(a, raw, True)
        p, text = pe_fixture(True); p[0x220] ^= 1
        with self.assertRaises(ValueError): self.pe(p, text, True)

    def test_full_end_must_fit_bss_and_pt_load(self):
        a, raw = arm_fixture()
        # An apparently valid start cannot rescue a PT_LOAD one byte short.
        struct.pack_into('<Q', a, 120 + 40, 0x21fff)
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture()
        # Control begins at +0x22000, so its final byte exceeds shortened BSS.
        struct.pack_into('<Q', a, SYMOFF + 9 * 24 + 8, ARM + 0x22fff)
        with self.assertRaises(ValueError): self.arm(a, raw)

    def test_full_end_must_fit_pe_section_and_image(self):
        p, text = pe_fixture()
        struct.pack_into('<I', p, PESH + 40 + 8, 0x20fff)
        with self.assertRaises(ValueError): self.pe(p, text)
        p, text = pe_fixture()
        struct.pack_into('<I', p, PEOPT + 56, 0x22fff)
        with self.assertRaises(ValueError): self.pe(p, text)

    def test_exact_symbol_size_alignment_and_pool_overlap(self):
        a, raw = arm_fixture()
        struct.pack_into('<Q', a, SYMOFF + 2 * 24 + 16, 0x1ffff)
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture()
        struct.pack_into('<Q', a, SYMOFF + 2 * 24 + 8, ARM + 0x2001)
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture()
        struct.pack_into('<Q', a, SYMOFF + 3 * 24 + 8, ARM + 0x23000)
        with self.assertRaises(ValueError): self.arm(a, raw)
        p, text = pe_fixture()
        text = text.replace('0000000140023000', '0000000140024000').replace(
            '0002:00021000 styleos_paging_control', '0002:00022000 styleos_paging_control')
        with self.assertRaises(ValueError): self.pe(p, text)

    def test_header_raw_bytes_and_initializer_are_bound(self):
        a, raw = arm_fixture(); raw[16] ^= 1
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture(); raw[0x1008] ^= 1
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture(); a[0x2000] ^= 1; raw[0x1000] ^= 1
        with self.assertRaises(ValueError): self.arm(a, raw)
        p, text = pe_fixture(); p[0x408] ^= 1
        with self.assertRaises(ValueError): self.pe(p, text)

    def test_nonidentity_load_and_wrong_section_types(self):
        a, raw = arm_fixture(); struct.pack_into('<Q', a, 120 + 24, ARM + 0x2000)
        with self.assertRaises(ValueError): self.arm(a, raw)
        a, raw = arm_fixture(); struct.pack_into('<I', a, SHOFF + 3 * 64 + 4, 1)
        with self.assertRaises(ValueError): self.arm(a, raw)
        p, text = pe_fixture(); struct.pack_into('<I', p, PESH + 40 + 36, 0x40000040)
        with self.assertRaises(ValueError): self.pe(p, text)

    def test_missing_duplicate_truncated_and_bad_sidecars(self):
        p, text = pe_fixture()
        table_row = next(line for line in text.splitlines() if 'styleos_paging_tables' in line)
        with self.assertRaises(ValueError): self.pe(p, text + table_row + '\n')
        with self.assertRaises(ValueError): self.pe(p, text.replace(table_row, ''))
        with self.assertRaises(ValueError): self.pe(p[:100], text)
        with self.assertRaises(ValueError): self.pe(p, text + '\0')
        a, raw = arm_fixture()
        with self.assertRaises(ValueError): self.arm(a[:0x700], raw)

    def test_required_sidecars_and_arch(self):
        empty = self.folder / 'empty'; empty.write_bytes(b'')
        for arch in ('x86_64', 'aarch64', 'm32'):
            with self.assertRaises(ValueError): describe_paging(arch, empty)
        with self.assertRaises(ValueError): describe_paging('x86_64', empty, fault=1)


if __name__ == '__main__':
    unittest.main()
