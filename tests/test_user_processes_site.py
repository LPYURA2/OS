"""Independent malformed PE/ELF fixtures for user-page and PC evidence."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.user_processes_site import (describe_user_processes, COMMON,
    X86_OBJECTS, FORBIDDEN, CODE_SYMBOLS, PAGE)

ARM_BASE = 0x40080000
PE_BASE = 0x140000000
TEXT_BYTES = 0x5000
ELF_TEXT = 0x10000
ELF_SYM = 0x1000
ELF_NAMES = 0x3000
ELF_SH = 0x500
PE_OPT = 0x98
PE_SH = PE_OPT + 112
ARM_FLAGS = ['-mgeneral-regs-only']
X86_FLAGS = ['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']


def storage(arch):
    rows = []
    address = 0x11000
    for key, (name, size, alignment) in {**COMMON,
            **(X86_OBJECTS if arch == 'x86_64' else {})}.items():
        address = (address + alignment - 1) & -alignment
        rows.append((name, address, size))
        address += size
    for name, size in [('styleos_bootpool_storage', 32768),
                       ('styleos_bootpool_metadata', PAGE)]:
        address = (address + PAGE - 1) & -PAGE
        rows.append((name, address, size))
        address += size
    address = (address + PAGE - 1) & -PAGE
    start = address
    rows += [('__stack_bottom' if arch == 'aarch64' else 'styleos_stack_bottom',
              start, 0),
             ('__stack_top' if arch == 'aarch64' else 'styleos_stack_top',
              start + 65536, 0)]
    address += 65536
    if arch == 'x86_64':
        rows += [('early_idt', address, PAGE), ('handoff', 0x10000, 96)]
        address += PAGE
    return rows, address


def programs():
    return [('styleos_user_code_a', 0x2000, PAGE),
            ('styleos_user_code_a_end', 0x3000, 0),
            ('styleos_user_code_b', 0x3000, PAGE),
            ('styleos_user_code_b_end', 0x4000, 0),
            ('styleos_user_a_start', 0x2000, 0),
            ('styleos_user_a_yield_resume', 0x2008, 0),
            ('styleos_user_a_fault', 0x2010, 0),
            ('styleos_user_b_start', 0x3000, 0),
            ('styleos_user_b_yield_resume', 0x3008, 0),
            ('styleos_user_b_exit', 0x3014, 0)]


def text_bytes(arch, image_size):
    raw = bytearray(bytes.fromhex('1f2003d5') * (TEXT_BYTES // 4)
                    if arch == 'aarch64' else b'\x90' * TEXT_BYTES)
    if arch == 'aarch64':
        raw[:64] = bytes(64)
        struct.pack_into('<I', raw, 0, 0x14000010)
        struct.pack_into('<QQQ', raw, 8, 0x80000, image_size, 0)
        struct.pack_into('<I', raw, 56, 0x644d5241)
        raw[0x2004:0x2008] = (0xd4000021).to_bytes(4, 'little')
        raw[0x3004:0x3008] = (0xd4000021).to_bytes(4, 'little')
        raw[0x2010:0x2014] = bytes.fromhex('010040f9')
        raw[0x3014:0x3018] = (0xd4000041).to_bytes(4, 'little')
    else:
        raw[0x2006:0x2008] = bytes.fromhex('cd80')
        raw[0x3006:0x3008] = bytes.fromhex('cd80')
        raw[0x2010:0x2013] = bytes.fromhex('488b07')
        raw[0x3014:0x3016] = bytes.fromhex('cd80')
    return raw


def arm_fixture(extra=None):
    stored, image_size = storage('aarch64')
    rows = [(name, 0x01 if name == 'diagnostic' else 0x11 if size else 0x10,
             2, value, size) for name, value, size in stored]
    rows += [('image_start', 0x10, 1, 0, 0),
             ('__image_end', 0x10, 2, image_size, 0),
             ('__bss_start', 0x10, 2, 0x10000, 0),
             ('__bss_end', 0x10, 2, image_size, 0),
             ('aarch64_vectors', 0x11, 1, 0x800, 2048),
             ('styleos_user_vectors', 0x11, 1, 0x1000, 2048)]
    rows += [(name, 0x11 if size else 0x10, 1, value, size)
             for name, value, size in programs()]
    if extra:
        rows.append((extra, 0x10, 1, 0x1800, 0))
    data = bytearray(ELF_TEXT + TEXT_BYTES)
    raw = text_bytes('aarch64', image_size)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM_BASE,
                     64, ELF_SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, ELF_TEXT,
                     ARM_BASE, ARM_BASE, TEXT_BYTES, TEXT_BYTES, PAGE)
    struct.pack_into('<IIQQQQQQ', data, 120, 1, 6, len(data),
                     ARM_BASE + 0x10000, ARM_BASE + 0x10000, 0,
                     image_size - 0x10000, PAGE)
    strings = b'\0' + b'\0'.join(row[0].encode() for row in rows) + b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[ELF_NAMES:ELF_NAMES + len(strings)] = strings
    data[0x4000:0x4000 + len(names)] = names
    offsets = {}
    for n, (name, info, section, value, size) in enumerate(rows, 1):
        off = ELF_SYM + n * 24
        offsets[name] = off
        struct.pack_into('<IBBHQQ', data, off,
                         strings.index(b'\0' + name.encode() + b'\0') + 1,
                         info, 0, section, ARM_BASE + value, size)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (names.index(b'.text'), 1, 6, ARM_BASE, ELF_TEXT,
                 TEXT_BYTES, 0, 0, PAGE, 0),
                (names.index(b'.bss'), 8, 3, ARM_BASE + 0x10000,
                 len(data), image_size - 0x10000, 0, 0, PAGE, 0),
                (names.index(b'.symtab'), 2, 0, 0, ELF_SYM,
                 (len(rows) + 1) * 24, 4, 1, 8, 24),
                (names.index(b'.strtab'), 3, 0, 0, ELF_NAMES,
                 len(strings), 0, 0, 1, 0),
                (names.index(b'.shstrtab'), 3, 0, 0, 0x4000,
                 len(names), 0, 0, 1, 0)]
    for n, row in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, ELF_SH + n * 64, *row)
    data[ELF_TEXT:] = raw
    return data, raw, offsets


def pe_fixture(**positions):
    stored, end = storage('x86_64')
    image_size = (end + PAGE - 1) & -PAGE
    data = bytearray(0x200 + TEXT_BYTES + 0x200)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 2, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PE_OPT, 0x20b)
    struct.pack_into('<Q', data, PE_OPT + 24, PE_BASE)
    struct.pack_into('<II', data, PE_OPT + 32, PAGE, 0x200)
    struct.pack_into('<II', data, PE_OPT + 56, image_size, 0x200)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH, b'.text\0\0\0',
                     TEXT_BYTES, PAGE, TEXT_BYTES, 0x200, 0, 0, 0, 0, 0x60000020)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH + 40, b'.data\0\0\0',
                     image_size - 0x10000, 0x10000, 0x200,
                     0x200 + TEXT_BYTES, 0, 0, 0, 0, 0xc0000040)
    data[0x200:0x200 + TEXT_BYTES] = text_bytes('x86_64', image_size)
    data[0x200 + TEXT_BYTES] = 0x53
    code_rows = [(n, a + PAGE, s) for n, a, s in programs()]
    all_rows = [(name, a, size, 2) for name, a, size in stored]
    all_rows += [(name, a, size, 1) for name, a, size in code_rows]
    statics = {'diagnostic': 'kernel_boot_memory_diagnostic.o',
               'handoff': 'boot_x86_64_uefi.o',
               'early_idt': 'arch_x86_64_exceptions.o'}
    text = (f'fixture\n Preferred load address is {PE_BASE:016x}\n\n'
            ' Address Publics by Value Rva+Base Lib:Object\n\n')
    for static in (False, True):
        if static:
            text += '\n Static symbols\n\n'
        for name, address, size, segment in all_rows:
            if (name in statics) != static:
                continue
            address = positions.get(name, address)
            section_start = PAGE if segment == 1 else 0x10000
            origin = statics.get(name, 'user_processes.o')
            text += (f' {segment:04x}:{address - section_start:016x} {name} '
                     f'{PE_BASE + address:016x} {origin}\n')
    return data, text


def fixture_decoder(arch, binary, spans, flags):
    data = Path(binary).read_bytes()
    decoded = {}
    for span in spans:
        current = span['start']
        while current < span['end']:
            offset = span['offset'] + current - span['start']
            size = 4 if arch == 'aarch64' else (2 if data[offset:offset + 2] == b'\xcd\x80'
                    else 3 if data[offset:offset + 3] == b'\x48\x8b\x07' else 1)
            decoded[current] = {'bytes': data[offset:offset + size].hex(),
                                'instruction': 'fixture', 'kind': 'instruction'}
            current += size
    return {'fixture': True}, decoded


class UserProcessSites(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        patcher = patch('tools.user_processes_site.verify_integer_code',
                        side_effect=fixture_decoder)
        patcher.start()
        self.addCleanup(patcher.stop)

    def arm(self, data=None, raw=None):
        default, transport, _ = arm_fixture()
        data = default if data is None else data
        raw = transport if raw is None else raw
        binary, image = self.folder / 'kernel.elf', self.folder / 'styleos.bin'
        binary.write_bytes(data)
        image.write_bytes(raw)
        return describe_user_processes('aarch64', binary, raw_path=image,
                                       compiler_flags=ARM_FLAGS)

    def pe(self, data=None, text=None):
        default, symbols = pe_fixture()
        data = default if data is None else data
        text = symbols if text is None else text
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(data)
        map_path.write_text(text)
        return describe_user_processes('x86_64', binary, map_path=map_path,
                                       compiler_flags=X86_FLAGS)

    def test_arm_complete_resources_and_user_pc_are_distinct_from_image(self):
        site = self.arm()
        self.assertEqual(site['objects']['tables']['bytes'], 3 * 32 * PAGE)
        self.assertEqual(site['objects']['control']['bytes'], 16384)
        self.assertEqual(site['objects']['user_vectors']['bytes'], 2048)
        self.assertEqual(site['programs']['A']['sites']['fault']['user_pc'], 0x10000010)
        self.assertEqual(site['programs']['A']['sites']['fault']['image_pc'], ARM_BASE + 0x2010)
        self.assertEqual(site['programs']['B']['sites']['exit_resume']['user_pc'], 0x10000018)
        self.assertEqual(site['programs']['A']['sites']['yield_resume']['call_site']['raw_hex'], '210000d4')
        self.assertEqual(site['fault_va'], 0x12010000)

    def test_pe_complete_code_is_file_backed_and_reports_rva(self):
        site = self.pe()
        self.assertEqual(site['objects']['code_a']['start'], 0x3000)
        self.assertEqual(site['objects']['gdt']['bytes'], PAGE)
        self.assertEqual(site['programs']['A']['sites']['fault']['image_rva'], 0x3010)
        self.assertEqual(site['programs']['B']['sites']['exit_resume']['user_pc'], 0x10000016)
        self.assertEqual(len(bytes.fromhex(site['programs']['A']['raw_hex'])), PAGE)
        raw = bytes.fromhex(site['programs']['A']['raw_hex'])
        self.assertEqual(site['programs']['A']['sha256'], hashlib.sha256(raw).hexdigest())

    def test_arm_refuses_wrong_storage_size_alignment_and_overlap(self):
        for name in ('styleos_user_tables', 'styleos_user_control',
                     'styleos_user_stack_a', 'styleos_user_entry_stack'):
            for mode in ('size', 'unaligned', 'overlap'):
                with self.subTest(name=name, mode=mode), self.assertRaises(ValueError):
                    data, raw, offsets = arm_fixture()
                    off = offsets[name]
                    if mode == 'size':
                        size = struct.unpack_from('<Q', data, off + 16)[0]
                        struct.pack_into('<Q', data, off + 16, size - 1)
                    else:
                        address = (ARM_BASE + 0x11001 if mode == 'unaligned'
                                   else ARM_BASE + 0x11000)
                        if name == 'styleos_user_tables' and mode == 'overlap':
                            address += PAGE
                        struct.pack_into('<Q', data, off + 8, address)
                    self.arm(data, raw)

    def test_arm_refuses_complete_tail_outside_bss_load_or_image(self):
        for offset in (ELF_SH + 2 * 64 + 32, 120 + 40):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data, raw, _ = arm_fixture()
                struct.pack_into('<Q', data, offset, PAGE)
                self.arm(data, raw)

    def test_code_pages_require_exact_full_aligned_extent(self):
        for name in ('styleos_user_code_a', 'styleos_user_code_b'):
            for mode in ('size', 'end', 'alignment'):
                with self.subTest(name=name, mode=mode), self.assertRaises(ValueError):
                    data, raw, offsets = arm_fixture()
                    off = offsets[name + '_end'] if mode == 'end' else offsets[name]
                    if mode == 'size':
                        struct.pack_into('<Q', data, off + 16, PAGE - 4)
                    else:
                        address = struct.unpack_from('<Q', data, off + 8)[0]
                        struct.pack_into('<Q', data, off + 8, address + 4)
                    self.arm(data, raw)

    def test_pe_refuses_crossing_file_tail_or_nonexecuting_page(self):
        for offset, value in ((PE_SH + 8, 0x3000),
                              (PE_SH + 16, 0x3000),
                              (PE_SH + 36, 0x40000040)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data, text = pe_fixture()
                struct.pack_into('<I', data, offset, value)
                self.pe(data, text)

    def test_pe_rejects_code_page_wrong_endpoint_alignment_and_overlap(self):
        for name, address in [('styleos_user_code_a_end', 0x3fff),
                              ('styleos_user_code_a', 0x3001),
                              ('styleos_user_code_b', 0x3000),
                              ('styleos_user_stack_b', 0x11000)]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.pe(*pe_fixture(**{name: address}))

    def test_refuses_missing_duplicate_or_unrelated_symbols(self):
        for extra in FORBIDDEN | {'styleos_user_code_a'}:
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                data, raw, _ = arm_fixture(extra)
                self.arm(data, raw)
        for name in CODE_SYMBOLS:
            with self.subTest(name=name), self.assertRaises(ValueError):
                data, raw, offsets = arm_fixture()
                struct.pack_into('<I', data, offsets[name], 0)
                self.arm(data, raw)

    def test_yield_and_exit_must_be_exact_hardware_traps(self):
        for offset in (0x2004, 0x3004, 0x3014):
            with self.subTest(arm_offset=offset), self.assertRaises(ValueError):
                data, raw, _ = arm_fixture()
                raw[offset:offset + 4] = bytes.fromhex('1f2003d5')
                data[ELF_TEXT + offset:ELF_TEXT + offset + 4] = raw[offset:offset + 4]
                self.arm(data, raw)
        for offset in (0x2006, 0x3006, 0x3014):
            with self.subTest(pe_offset=offset), self.assertRaises(ValueError):
                data, text = pe_fixture()
                data[0x200 + offset:0x202 + offset] = b'\x90\x90'
                self.pe(data, text)

    def test_fault_read_is_exact_and_pc_must_belong_to_own_code_page(self):
        data, raw, _ = arm_fixture()
        raw[0x2010:0x2014] = bytes.fromhex('1f2003d5')
        data[ELF_TEXT + 0x2010:ELF_TEXT + 0x2014] = raw[0x2010:0x2014]
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        for name, target in [('styleos_user_a_fault', ARM_BASE + 0x3010),
                              ('styleos_user_b_exit', ARM_BASE + 0x2014),
                              ('styleos_user_a_start', ARM_BASE + 0x2004)]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                data, raw, offsets = arm_fixture()
                struct.pack_into('<Q', data, offsets[name] + 8, target)
                self.arm(data, raw)

    def test_raw_image_and_full_binary_must_match(self):
        data, raw, _ = arm_fixture()
        raw[0x3fff] ^= 1
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        with self.assertRaises(ValueError):
            self.pe(pe_fixture()[0][:-1])

    def test_decoder_gap_or_padding_in_user_page_is_refused(self):
        for mode in ('gap', 'padding'):
            def malformed(*args):
                proof, decoded = fixture_decoder(*args)
                if mode == 'gap':
                    del decoded[ARM_BASE + 0x2040]
                else:
                    decoded[ARM_BASE + 0x2040]['kind'] = 'zero-padding'
                return proof, decoded
            with self.subTest(mode=mode), patch(
                    'tools.user_processes_site.verify_integer_code', side_effect=malformed):
                with self.assertRaises(ValueError):
                    self.arm()

    def test_requires_regular_files_and_arch_dependencies(self):
        empty = self.folder / 'empty'
        empty.write_bytes(b'')
        for arch in ('aarch64', 'x86_64', 'unknown'):
            with self.subTest(arch=arch), self.assertRaises(ValueError):
                describe_user_processes(arch, empty)


if __name__ == '__main__':
    unittest.main()
