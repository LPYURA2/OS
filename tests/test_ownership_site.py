"""Independent ELF/PE fixtures for complete ownership-profile object proofs."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.ownership_site import describe_ownership, WORKSPACE_SYMBOL, FORBIDDEN

ARM_BASE = 0x40080000
PE_BASE = 0x140000000
ELF_PH = 64
ELF_SYM = 0x200
ELF_SH = 0x1000
PE_OPT = 0x98
PE_SH = PE_OPT + 112
POOL = 'styleos_bootpool_storage'
META = 'styleos_bootpool_metadata'
SYMBOLS = (WORKSPACE_SYMBOL, POOL, META, 'image_start', '__image_end',
           '__bss_start', '__bss_end', 'diagnostic', '__stack_bottom',
           '__stack_top', 'aarch64_vectors')


def elf_symbol_offset(name):
    return ELF_SYM + 24 * (1 + SYMBOLS.index(name))


def arm_fixture(extra=None):
    data = bytearray(0x3000)
    raw = bytearray(0x1000)
    raw[:4] = bytes.fromhex('10000014')
    raw[0x800:] = bytes.fromhex('1f2003d5') * 512
    struct.pack_into('<QQQ', raw, 8, 0x80000, 0x2e080, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM_BASE,
                     ELF_PH, ELF_SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH, 1, 5, 0x2000,
                     ARM_BASE, ARM_BASE, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH + 56, 1, 6, 0x3000,
                     ARM_BASE + 0x2000, ARM_BASE + 0x2000, 0, 0x2c080, 0x1000)
    data[0x2000:0x3000] = raw
    labels = SYMBOLS + (() if extra is None else (extra,))
    strings = b'\0' + b'\0'.join(name.encode() for name in labels) + b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[0x700:0x700 + len(strings)] = strings
    data[0xc00:0xc00 + len(names)] = names
    rows = [(WORKSPACE_SYMBOL, 0x11, 2, 0x2000, 65536),
            (POOL, 0x11, 2, 0x12000, 32768),
            (META, 0x11, 2, 0x1a000, 4096),
            ('image_start', 0x10, 1, 0, 0),
            ('__image_end', 0x10, 2, 0x2e080, 0),
            ('__bss_start', 0x10, 2, 0x2000, 0),
            ('__bss_end', 0x10, 2, 0x2e080, 0),
            ('diagnostic', 0x01, 2, 0x1b000, 12400),
            ('__stack_bottom', 0x10, 2, 0x1e080, 0),
            ('__stack_top', 0x10, 2, 0x2e080, 0),
            ('aarch64_vectors', 0x11, 1, 0x800, 2048)]
    if extra is not None:
        rows.append((extra, 0x10, 1, 0x100, 0))
    for n, (name, info, section, value, size) in enumerate(rows, 1):
        struct.pack_into('<IBBHQQ', data, ELF_SYM + 24 * n,
                         strings.index(name.encode()), info, 0, section,
                         ARM_BASE + value, size)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (names.index(b'.text'), 1, 6, ARM_BASE, 0x2000, 0x1000, 0, 0, 2048, 0),
                (names.index(b'.bss'), 8, 3, ARM_BASE + 0x2000, 0x3000, 0x2c080,
                 0, 0, 4096, 0),
                (names.index(b'.symtab'), 2, 0, 0, ELF_SYM, (len(rows) + 1) * 24,
                 4, 1, 8, 24),
                (names.index(b'.strtab'), 3, 0, 0, 0x700, len(strings), 0, 0, 1, 0),
                (names.index(b'.shstrtab'), 3, 0, 0, 0xc00, len(names), 0, 0, 1, 0)]
    for n, row in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, ELF_SH + n * 64, *row)
    return data, raw


def pe_fixture(raw_bytes=0x200, base=PE_BASE):
    data = bytearray(0x200 + raw_bytes)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 1, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PE_OPT, 0x20b)
    struct.pack_into('<Q', data, PE_OPT + 24, base)
    struct.pack_into('<II', data, PE_OPT + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, PE_OPT + 56, 0x30000, 0x200)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH, b'.data\0\0\0',
                     0x2e080, 0x1000, raw_bytes, 0x200 if raw_bytes else 0,
                     0, 0, 0, 0, 0xc0000040)
    if raw_bytes:
        # Handoff is initialized; no BSS object uses these bytes.
        data[0x200] = 0x53
    return data


def map_fixture(base=PE_BASE, **positions):
    rows = [(WORKSPACE_SYMBOL, 0x2000, 'kernel_ownership_plan.o'),
            (POOL, 0x12000, 'kernel_boot_pool.o'),
            (META, 0x1a000, 'kernel_boot_pool.o'),
            ('styleos_stack_bottom', 0x1e080, 'arch_x86_64_entry.o'),
            ('styleos_stack_top', 0x2e080, 'arch_x86_64_entry.o'),
            ('handoff', 0x1000, 'boot_x86_64_uefi.o'),
            ('diagnostic', 0x1b000, 'kernel_boot_memory_diagnostic.o'),
            ('early_idt', 0x2e080, 'arch_x86_64_exceptions.o')]
    text = (f'fixture\n Preferred load address is {base:016x}\n\n'
            ' Address Publics by Value Rva+Base Lib:Object\n\n')
    for n, (name, default, origin) in enumerate(rows):
        if n == 5:
            text += '\n Static symbols\n\n'
        address = positions.get(name, default)
        text += f' 0001:{address - 0x1000:08x} {name} {base + address:016x} {origin}\n'
    return text


class OwnershipSiteTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='styleos-ownership-site-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def arm(self, data=None, raw=None, invalid=False):
        defaults = arm_fixture()
        data = defaults[0] if data is None else data
        raw = defaults[1] if raw is None else raw
        binary, raw_path = self.folder / 'kernel.elf', self.folder / 'styleos.bin'
        binary.write_bytes(data)
        raw_path.write_bytes(raw)
        return describe_ownership('aarch64', binary, raw_path=raw_path, invalid=invalid)

    def pe(self, data=None, text=None, invalid=False):
        data = pe_fixture() if data is None else data
        text = map_fixture() if text is None else text
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(data)
        map_path.write_text(text)
        return describe_ownership('x86_64', binary, map_path, invalid=invalid)

    def test_arm_complete_workspace_stack_vectors_and_input_hashes(self):
        result = self.arm()
        self.assertEqual(result['address_space'], 'identity-image-address')
        self.assertEqual(result['objects']['workspace'], dict(symbol=WORKSPACE_SYMBOL,
            start=ARM_BASE + 0x2000, end=ARM_BASE + 0x12000, bytes=65536))
        self.assertEqual(result['objects']['stack']['end'], result['image_end'])
        self.assertEqual(result['objects']['stack']['end'], result['bss_end'])
        self.assertEqual(result['objects']['diagnostic']['bytes'], 12400)
        self.assertEqual(result['objects']['vectors']['bytes'], 2048)
        self.assertEqual(set(result['objects']),
                         {'workspace', 'diagnostic', 'stack', 'pool', 'pool_metadata', 'vectors'})
        data, raw = arm_fixture()
        self.assertEqual(result['binary_sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(result['raw_sha256'], hashlib.sha256(raw).hexdigest())
        self.assertFalse(result['invalid'])

    def test_pe_rvas_static_symbols_and_input_hashes(self):
        result = self.pe()
        self.assertEqual(result['address_space'], 'image-rva')
        self.assertEqual(result['preferred_image_base'], PE_BASE)
        self.assertEqual(result['objects']['workspace']['end'], 0x12000)
        self.assertEqual(result['objects']['handoff']['bytes'], 96)
        self.assertEqual(result['objects']['idt']['bytes'], 4096)
        self.assertEqual(set(result['objects']),
                         {'workspace', 'diagnostic', 'stack', 'pool', 'pool_metadata', 'idt', 'handoff'})
        self.assertEqual(result['binary_sha256'], hashlib.sha256(pe_fixture()).hexdigest())
        self.assertEqual(result['map_sha256'], hashlib.sha256(map_fixture().encode()).hexdigest())
        self.assertNotIn('runtime_base', result)

    def test_invalid_profile_keeps_full_static_proof(self):
        self.assertTrue(self.arm(invalid=True)['invalid'])
        self.assertTrue(self.pe(invalid=True)['invalid'])
        with self.assertRaises(ValueError):
            self.pe(text=map_fixture(**{WORKSPACE_SYMBOL: 0x29000}), invalid=True)
        with self.assertRaises(ValueError):
            self.arm(invalid=1)

    def test_requires_arch_sidecars_regular_files_and_full_headers(self):
        binary = self.folder / 'binary'
        binary.write_bytes(b'')
        for arch in ('aarch64', 'x86_64', 'unsupported'):
            with self.subTest(arch=arch), self.assertRaises(ValueError):
                describe_ownership(arch, binary)
        with self.assertRaises(ValueError):
            describe_ownership('aarch64', self.folder, raw_path=binary)
        for data in (b'', arm_fixture()[0][:63], arm_fixture()[0][:0x2fff]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                self.arm(data)
        with self.assertRaises(ValueError):
            self.pe(pe_fixture()[:63])

    def test_arm_rejects_wrong_workspace_size_alignment_kind_and_binding(self):
        off = elf_symbol_offset(WORKSPACE_SYMBOL)
        cases = [(off + 16, '<Q', 65535), (off + 16, '<Q', 65537),
                 (off + 8, '<Q', ARM_BASE + 0x2001),
                 (off + 4, '<B', 0x10), (off + 4, '<B', 0x01),
                 (off + 6, '<H', 0), (off + 8, '<Q', (1 << 64) - 4096)]
        for offset, fmt, value in cases:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.arm(data)

    def test_arm_rejects_workspace_full_end_misplacement_and_overlaps(self):
        for target in (0x12000, 0x1a000, 0x1b000, 0x1f000, 0x28000):
            with self.subTest(target=target), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into('<Q', data, elf_symbol_offset(WORKSPACE_SYMBOL) + 8,
                                 ARM_BASE + target)
                self.arm(data)

    def test_arm_rejects_truncated_bss_section_load_and_image_tail(self):
        cases = [(ELF_SH + 2 * 64 + 32, '<Q', 0x2c07f),
                 (ELF_PH + 56 + 40, '<Q', 0x2c07f),
                 (ELF_PH + 56 + 4, '<I', 4),
                 (ELF_PH + 56 + 24, '<Q', ARM_BASE + 0x3000),
                 (elf_symbol_offset('__bss_end') + 8, '<Q', ARM_BASE + 0x2e070),
                 (elf_symbol_offset('__image_end') + 8, '<Q', ARM_BASE + 0x2e070),
                 (ELF_SH + 2 * 64 + 4, '<I', 1),
                 (ELF_SH + 2 * 64 + 8, '<Q', 2)]
        for offset, fmt, value in cases:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.arm(data)

    def test_arm_rejects_wrong_diagnostic_stack_and_executable_vectors(self):
        cases = [(elf_symbol_offset('diagnostic') + 16, '<Q', 12399),
                 (elf_symbol_offset('diagnostic') + 8, '<Q', ARM_BASE + 0x1b001),
                 (elf_symbol_offset('__stack_top') + 8, '<Q', ARM_BASE + 0x2e070),
                 (elf_symbol_offset('__stack_bottom') + 8, '<Q', ARM_BASE + 0x1e081),
                 (elf_symbol_offset('aarch64_vectors') + 16, '<Q', 2047),
                 (elf_symbol_offset('aarch64_vectors') + 8, '<Q', ARM_BASE + 0x801),
                 (ELF_PH + 4, '<I', 4), (ELF_SH + 64 + 8, '<Q', 2)]
        for offset, fmt, value in cases:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.arm(data)

    def test_arm_missing_duplicate_and_static_local_diagnostic(self):
        data, raw = arm_fixture()
        for name in (WORKSPACE_SYMBOL, 'diagnostic', '__stack_top', 'aarch64_vectors'):
            with self.subTest(missing=name), self.assertRaises(ValueError):
                bad = bytearray(data)
                struct.pack_into('<I', bad, elf_symbol_offset(name), 0)
                self.arm(bad, raw)
        data, raw = arm_fixture(extra=WORKSPACE_SYMBOL)
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        data, raw = arm_fixture()
        struct.pack_into('<B', data, elf_symbol_offset('diagnostic') + 4, 0x11)
        self.arm(data, raw)  # A global diagnostic symbol is also unambiguous.

    def test_arm_raw_header_and_executable_content_must_match(self):
        for offset, fmt, value in ((8, '<Q', 0), (16, '<Q', 0x2e070), (56, '<I', 0)):
            with self.subTest(header=offset), self.assertRaises(ValueError):
                data, raw = arm_fixture()
                struct.pack_into(fmt, raw, offset, value)
                struct.pack_into(fmt, data, 0x2000 + offset, value)
                self.arm(data, raw)
        data, raw = arm_fixture()
        raw[0xfff] ^= 1
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        with self.assertRaises(ValueError):
            self.arm(raw=arm_fixture()[1][:63])

    def test_pe_rejects_workspace_full_end_misplacement_alignment_and_overlap(self):
        for address in (0x2001, 0x12000, 0x1a000, 0x1b000, 0x1f000, 0x29000):
            with self.subTest(address=address), self.assertRaises(ValueError):
                self.pe(text=map_fixture(**{WORKSPACE_SYMBOL: address}))
        for name, address in (('diagnostic', 0x2000), ('styleos_stack_top', 0x2e070),
                              ('early_idt', 0x2e070), ('handoff', 0x1b000)):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.pe(text=map_fixture(**{name: address}))

    def test_pe_rejects_section_image_and_header_truncation(self):
        cases = [(PE_SH + 8, '<I', 0x2e07f),
                 (PE_OPT + 56, '<I', 0x2f07f),
                 (PE_SH + 36, '<I', 0x40000040),
                 (PE_SH + 36, '<I', 0xe0000020),
                 (PE_OPT + 60, '<I', PE_SH + 39)]
        for offset, fmt, value in cases:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                data = pe_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.pe(data)
        with self.assertRaises(ValueError):
            self.pe(pe_fixture()[:-1])
        with self.assertRaises(ValueError):
            self.pe(pe_fixture(raw_bytes=0))

    def test_pe_zero_prefix_and_virtual_tail_prove_complete_workspace(self):
        data = pe_fixture(raw_bytes=0x1200)
        self.pe(data)  # First 512 workspace bytes exist in the file and are zero.
        data[0x200 + 0x1000 + 0x1ff] = 1
        with self.assertRaises(ValueError):
            self.pe(data)
        result = self.pe(pe_fixture(base=0), map_fixture(base=0))
        self.assertEqual(result['preferred_image_base'], 0)

    def test_pe_static_origin_disambiguation_and_bad_map_rows(self):
        text = map_fixture()
        duplicate = text.splitlines()[-2]
        self.pe(text=text + duplicate.replace('kernel_boot_memory_diagnostic.o', 'another.o') + '\n')
        cases = [text.replace('kernel_boot_memory_diagnostic.o', 'another.o'),
                 text + duplicate + '\n', text.replace('Static symbols', 'Other symbols'),
                 text.replace('Rva+Base', 'RuntimeBase'), text + '\0',
                 text.replace(WORKSPACE_SYMBOL, 'other_workspace'),
                 text.replace('0001:00001000', '0002:00001000'),
                 text.replace('0001:00001000', '0001:00001001'),
                 text.replace('Preferred load address is 0000000140000000',
                              'Preferred load address is 0000000180000000')]
        for n, case in enumerate(cases):
            with self.subTest(case=n), self.assertRaises(ValueError):
                self.pe(text=case)

    def test_rejects_paging_buffers_and_fault_symbols_in_either_profile(self):
        for symbol in FORBIDDEN:
            for invalid in (False, True):
                with self.subTest(symbol=symbol, invalid=invalid):
                    data, raw = arm_fixture(extra=symbol)
                    with self.assertRaises(ValueError):
                        self.arm(data, raw, invalid)
                    text = map_fixture() + f' 0001:00000000 {symbol} {PE_BASE + 0x1000:016x} other.o\n'
                    with self.assertRaises(ValueError):
                        self.pe(text=text, invalid=invalid)

    def test_overlapping_loads_and_sections_are_not_complete_unique_proofs(self):
        data, raw = arm_fixture()
        struct.pack_into('<H', data, 56, 3)
        struct.pack_into('<IIQQQQQQ', data, ELF_PH + 2 * 56, 1, 4, 0x3000,
                         ARM_BASE + 0x2e07f, ARM_BASE + 0x2e07f, 0, 1, 1)
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        data = pe_fixture()
        struct.pack_into('<H', data, 0x86, 2)
        struct.pack_into('<8sIIIIIIHHI', data, PE_SH + 40, b'.alias',
                         1, 0x2f07f, 0, 0, 0, 0, 0, 0, 0x40000080)
        with self.assertRaises(ValueError):
            self.pe(data)

    def test_sidecar_hash_changes_with_equivalent_map_evidence(self):
        before = self.pe()
        text = map_fixture() + '\n harmless map footer\n'
        after = self.pe(text=text)
        self.assertEqual(before['objects'], after['objects'])
        self.assertEqual(before['binary_sha256'], after['binary_sha256'])
        self.assertNotEqual(before['map_sha256'], after['map_sha256'])
        self.assertEqual(after['map_sha256'], hashlib.sha256(text.encode()).hexdigest())


if __name__ == '__main__':
    unittest.main()
