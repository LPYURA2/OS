"""Independent ELF/PE fixtures for whole task objects and executable sites."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.tasks_site import describe_tasks, CODE_SYMBOLS, CODE_EXTENTS, FORBIDDEN

ARM_BASE = 0x40080000
PE_BASE = 0x140000000
ELF_SYM, ELF_SH, ELF_PH = 0x200, 0x1000, 64
PE_OPT, PE_SH = 0x98, 0x98 + 112
X86_FLAGS = ['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
ARM_FLAGS = ['-mgeneral-regs-only']
STORAGE = [('styleos_tasks_stack_a', 0x2000, 16384),
           ('styleos_tasks_stack_b', 0x6000, 16384),
           ('styleos_tasks_control', 0xa000, 4096),
           ('styleos_bootpool_storage', 0xb000, 32768),
           ('styleos_bootpool_metadata', 0x13000, 4096),
           ('diagnostic', 0x14000, 12400)]
CODE = [('styleos_tasks_bootstrap', 0x100, 4),
        ('styleos_tasks_a', 0x104, 4), ('styleos_tasks_b', 0x108, 4),
        ('tasks_arch_switch', 0x110, 8), ('tasks_arch_switch_end', 0x118, 0),
        ('tasks_arch_probe', 0x120, 16), ('styleos_tasks_probe_resume', 0x12c, 0),
        ('tasks_arch_probe_end', 0x130, 0), ('tasks_arch_finish', 0x140, 16),
        ('styleos_tasks_exit_resume', 0x14c, 0), ('tasks_arch_finish_end', 0x150, 0),
        ('styleos_tasks_trap_return', 0x160, 4),
        ('styleos_tasks_trap_return_end', 0x164, 0),
        ('tasks_arch_dispatch', 0x180, 16),
        ('styleos_tasks_dispatch_resume', 0x18c, 0),
        ('tasks_arch_dispatch_end', 0x190, 0)]
RESUMES = (0x12c, 0x14c, 0x18c)
ARM_ROWS = ([(n, 0x01 if n == 'diagnostic' else 0x11, 2, a, s)
             for n, a, s in STORAGE] +
            [('image_start', 0x10, 1, 0, 0), ('__image_end', 0x10, 2, 0x27080, 0),
             ('__bss_start', 0x10, 2, 0x2000, 0),
             ('__bss_end', 0x10, 2, 0x27080, 0),
             ('__stack_bottom', 0x10, 2, 0x17080, 0),
             ('__stack_top', 0x10, 2, 0x27080, 0),
             ('aarch64_vectors', 0x11, 1, 0x800, 2048)] +
            [(n, 0x12 if s else 0x10, 1, a, s) for n, a, s in CODE])


def symbol_offset(name):
    return ELF_SYM + 24 * (1 + next(n for n, row in enumerate(ARM_ROWS) if row[0] == name))


def arm_fixture(extra=None):
    data = bytearray(0x3000)
    raw = bytearray(bytes.fromhex('1f2003d5') * 1024)
    raw[:64] = bytes(64)
    raw[:4] = bytes.fromhex('10000014')
    struct.pack_into('<QQQ', raw, 8, 0x80000, 0x27080, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    for resume in RESUMES:
        call = resume - 4
        word = 0x94000000 | (((0x110 - call) // 4) & 0x3ffffff)
        struct.pack_into('<I', raw, call, word)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, ARM_BASE,
                     ELF_PH, ELF_SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH, 1, 5, 0x2000,
                     ARM_BASE, ARM_BASE, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, ELF_PH + 56, 1, 6, 0x3000,
                     ARM_BASE + 0x2000, ARM_BASE + 0x2000, 0, 0x25080, 0x1000)
    data[0x2000:0x3000] = raw
    rows = ARM_ROWS + ([] if extra is None else [(extra, 0x10, 1, 0x200, 0)])
    strings = b'\0' + b'\0'.join(row[0].encode() for row in rows) + b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[0x700:0x700 + len(strings)] = strings
    data[0xe00:0xe00 + len(names)] = names
    for n, (name, info, section, value, size) in enumerate(rows, 1):
        struct.pack_into('<IBBHQQ', data, ELF_SYM + 24 * n,
                         strings.index(b'\0' + name.encode() + b'\0') + 1,
                         info, 0, section, ARM_BASE + value, size)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (names.index(b'.text'), 1, 6, ARM_BASE, 0x2000, 0x1000, 0, 0, 2048, 0),
                (names.index(b'.bss'), 8, 3, ARM_BASE + 0x2000, 0x3000,
                 0x25080, 0, 0, 4096, 0),
                (names.index(b'.symtab'), 2, 0, 0, ELF_SYM, (len(rows) + 1) * 24,
                 4, 1, 8, 24),
                (names.index(b'.strtab'), 3, 0, 0, 0x700, len(strings), 0, 0, 1, 0),
                (names.index(b'.shstrtab'), 3, 0, 0, 0xe00, len(names), 0, 0, 1, 0)]
    for n, row in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, ELF_SH + n * 64, *row)
    return data, raw


def pe_fixture():
    data = bytearray(0x1400)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x8664, 2, 0, 0, 0, 112, 0x22)
    struct.pack_into('<H', data, PE_OPT, 0x20b)
    struct.pack_into('<Q', data, PE_OPT + 24, PE_BASE)
    struct.pack_into('<II', data, PE_OPT + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, PE_OPT + 56, 0x2a000, 0x200)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH, b'.text\0\0\0',
                     0x1000, 0x1000, 0x1000, 0x200, 0, 0, 0, 0, 0x60000020)
    struct.pack_into('<8sIIIIIIHHI', data, PE_SH + 40, b'.data\0\0\0',
                     0x27080, 0x2000, 0x200, 0x1200, 0, 0, 0, 0, 0xc0000040)
    data[0x200:0x1200] = b'\x90' * 0x1000
    for resume in RESUMES:
        offset = 0x200 + resume - 5
        data[offset:offset + 5] = b'\xe8' + (0x110 - resume).to_bytes(4, 'little', signed=True)
    data[0x1200] = 0x53
    return data


def map_fixture(**positions):
    rows = [(n, a + 0x1000, 2, 'kernel_tasks_demo.o') for n, a, _ in STORAGE[:3]]
    rows += [(n, a + 0x1000, 2, 'kernel_boot_pool.o') for n, a, _ in STORAGE[3:5]]
    rows += [('styleos_stack_bottom', 0x18080, 2, 'arch_x86_64_entry.o'),
             ('styleos_stack_top', 0x28080, 2, 'arch_x86_64_entry.o')]
    rows += [(n, a + 0x1000, 1, 'arch_x86_64_tasks_context.o') for n, a, _ in CODE]
    rows += [('handoff', 0x2000, 2, 'boot_x86_64_uefi.o'),
             ('diagnostic', 0x15000, 2, 'kernel_boot_memory_diagnostic.o'),
             ('early_idt', 0x28080, 2, 'arch_x86_64_exceptions.o')]
    text = (f'fixture\n Preferred load address is {PE_BASE:016x}\n\n'
            ' Address Publics by Value Rva+Base Lib:Object\n\n')
    for n, (name, default, segment, origin) in enumerate(rows):
        if name == 'handoff':
            text += '\n Static symbols\n\n'
        address = positions.get(name, default)
        relative = address - (0x1000 if segment == 1 else 0x2000)
        text += f' {segment:04x}:{relative:08x} {name} {PE_BASE + address:016x} {origin}\n'
    return text


def fixture_decoder(arch, binary, spans, flags):
    """Decoder integration has independent negative tests in integer_code."""
    data = Path(binary).read_bytes()
    decoded = {}
    for span in spans:
        step = 4 if arch == 'aarch64' else 1
        address = span['start']
        while address < span['end']:
            offset = span['offset'] + address - span['start']
            width = 5 if arch == 'x86_64' and data[offset] == 0xe8 else step
            decoded[address] = {'bytes': data[offset:offset + width].hex(),
                                'instruction': 'nop'}
            address += width
    return {'compiler_flags': flags, 'decoder_version': 'fixture'}, decoded


class TasksSiteTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='styleos-tasks-site-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.decoder = patch('tools.tasks_site.verify_integer_code', side_effect=fixture_decoder)
        self.decoder.start()
        self.addCleanup(self.decoder.stop)

    def arm(self, data=None, raw=None):
        default = arm_fixture()
        data, raw = (default[0] if data is None else data,
                     default[1] if raw is None else raw)
        binary, raw_path = self.folder / 'kernel.elf', self.folder / 'styleos.bin'
        binary.write_bytes(data)
        raw_path.write_bytes(raw)
        return describe_tasks('aarch64', binary, raw_path=raw_path, compiler_flags=ARM_FLAGS)

    def pe(self, data=None, text=None):
        binary, map_path = self.folder / 'BOOTX64.EFI', self.folder / 'kernel.map'
        binary.write_bytes(pe_fixture() if data is None else data)
        map_path.write_text(map_fixture() if text is None else text)
        return describe_tasks('x86_64', binary, map_path, compiler_flags=X86_FLAGS)

    def test_arm_complete_bss_extents_and_bound_executable_bytes(self):
        result = self.arm()
        self.assertEqual(set(result['objects']), {'stack_a', 'stack_b', 'control',
                         'diagnostic', 'stack', 'pool', 'pool_metadata', 'vectors'})
        for name in ('stack_a', 'stack_b'):
            self.assertEqual(result['objects'][name]['bytes'], 16384)
            self.assertEqual(result['objects'][name]['start'] % 4096, 0)
        self.assertEqual(result['objects']['control']['bytes'], 4096)
        self.assertEqual(result['objects']['stack']['end'], result['bss_end'])
        self.assertEqual(result['objects']['diagnostic']['bytes'], 12400)
        self.assertEqual(result['objects']['vectors']['bytes'], 2048)
        self.assertEqual(result['binary_sha256'], hashlib.sha256(arm_fixture()[0]).hexdigest())
        self.assertEqual(result['raw_sha256'], hashlib.sha256(arm_fixture()[1]).hexdigest())
        switch = result['code_sites']['tasks_arch_switch']
        self.assertEqual(switch['pc'], ARM_BASE + 0x110)
        self.assertEqual(switch['bytes'], 8)
        self.assertEqual(switch['raw_hex'], '1f2003d5' * 2)
        self.assertEqual(set(result['code_sites']), set(CODE_SYMBOLS) | set(CODE_EXTENTS))
        resume = result['code_sites']['styleos_tasks_probe_resume']
        self.assertEqual(resume['call_site']['end'], resume['start'])
        self.assertEqual(resume['call_site']['target'], switch['start'])

    def test_pe_uses_rvas_and_binds_map_and_file_bytes(self):
        result = self.pe()
        self.assertEqual(result['address_space'], 'image-rva')
        self.assertEqual(result['preferred_image_base'], PE_BASE)
        self.assertEqual(result['objects']['stack_a']['start'], 0x3000)
        self.assertEqual(result['objects']['handoff']['bytes'], 96)
        self.assertEqual(result['objects']['idt']['bytes'], 4096)
        self.assertNotIn('runtime_base', result)
        self.assertEqual(result['map_sha256'], hashlib.sha256(map_fixture().encode()).hexdigest())
        switch = result['code_sites']['tasks_arch_switch']
        self.assertEqual(switch['rva'], 0x1110)
        self.assertEqual(switch['raw_hex'], '90' * 8)

    def test_arm_rejects_wrong_stack_control_sizes_alignment_and_binding(self):
        for name in ('styleos_tasks_stack_a', 'styleos_tasks_stack_b', 'styleos_tasks_control'):
            offset = symbol_offset(name)
            size = next(row[4] for row in ARM_ROWS if row[0] == name)
            for delta, fmt, value in ((16, '<Q', size - 1), (16, '<Q', size + 1),
                                       (8, '<Q', ARM_BASE + 0x2001), (4, '<B', 0x01),
                                       (4, '<B', 0x10), (6, '<H', 0)):
                with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                    data, _ = arm_fixture()
                    struct.pack_into(fmt, data, offset + delta, value)
                    self.arm(data)

    def test_arm_rejects_full_tail_outside_bss_load_or_image(self):
        cases = [(ELF_SH + 2 * 64 + 32, '<Q', 0x2507f),
                 (ELF_PH + 56 + 40, '<Q', 0x2507f),
                 (ELF_PH + 56 + 4, '<I', 4),
                 (ELF_PH + 56 + 24, '<Q', ARM_BASE + 0x3000),
                 (symbol_offset('__bss_end') + 8, '<Q', ARM_BASE + 0x27070),
                 (symbol_offset('__stack_top') + 8, '<Q', ARM_BASE + 0x27070)]
        for offset, fmt, value in cases:
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into(fmt, data, offset, value)
                self.arm(data)

    def test_arm_rejects_overlap_with_each_other_live_image_object(self):
        for target in (0x6000, 0xa000, 0xb000, 0x13000, 0x14000, 0x18000):
            with self.subTest(target=target), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into('<Q', data, symbol_offset('styleos_tasks_stack_a') + 8,
                                 ARM_BASE + target)
                self.arm(data)

    def test_arm_rejects_missing_duplicate_and_unrelated_profile_symbols(self):
        for name in ('styleos_tasks_stack_a', 'styleos_tasks_control', 'styleos_tasks_a',
                     'styleos_tasks_probe_resume'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into('<I', data, symbol_offset(name), 0)
                self.arm(data)
        for extra in ('styleos_tasks_stack_a', *FORBIDDEN):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.arm(*arm_fixture(extra))

    def test_arm_rejects_raw_mismatch_and_bad_executable_symbol(self):
        data, raw = arm_fixture()
        raw[0x110] ^= 1
        with self.assertRaises(ValueError):
            self.arm(data, raw)
        for name in ('tasks_arch_switch', 'styleos_tasks_bootstrap', 'styleos_tasks_exit_resume'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                data, _ = arm_fixture()
                struct.pack_into('<Q', data, symbol_offset(name) + 8, ARM_BASE + 0x101)
                self.arm(data)

    def test_arm_header_data_is_explicitly_excluded_from_decoder_spans(self):
        with patch('tools.tasks_site.verify_integer_code', side_effect=fixture_decoder) as decoder:
            self.arm()
        spans = decoder.call_args.args[2]
        self.assertEqual([(s['start'], s['end']) for s in spans],
                         [(ARM_BASE, ARM_BASE + 4), (ARM_BASE + 64, ARM_BASE + 0x1000)])

    def test_arm_requires_entry_branch_around_excluded_header_data(self):
        for word in (0xd503201f, 0x14000001, 0x94000010, 0x17ffffff):
            with self.subTest(word=word), self.assertRaises(ValueError):
                data, raw = arm_fixture()
                struct.pack_into('<I', raw, 0, word)
                struct.pack_into('<I', data, 0x2000, word)
                self.arm(data, raw)

    def test_resume_sites_require_real_immediate_call_to_same_switch(self):
        for resume in RESUMES:
            for word in (0xd503201f, 0x94000000):
                with self.subTest(resume=resume, word=word), self.assertRaises(ValueError):
                    data, raw = arm_fixture()
                    struct.pack_into('<I', raw, resume - 4, word)
                    struct.pack_into('<I', data, 0x2000 + resume - 4, word)
                    self.arm(data, raw)
            with self.subTest(pe_resume=resume), self.assertRaises(ValueError):
                data = pe_fixture()
                data[0x200 + resume - 4:0x200 + resume] = bytes(4)
                self.pe(data)

    def test_pe_rejects_alignment_overlap_full_tail_and_code_relocation_mismatch(self):
        for name, value in (('styleos_tasks_stack_a', 0x3001),
                            ('styleos_tasks_stack_a', 0x7000),
                            ('styleos_tasks_stack_a', 0x26000),
                            ('styleos_tasks_control', 0xc000),
                            ('styleos_tasks_a', 0x3000),
                            ('styleos_stack_top', 0x28070)):
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                self.pe(text=map_fixture(**{name: value}))

    def test_pe_rejects_nonzero_task_initial_bytes_and_missing_handoff_storage(self):
        data = pe_fixture()
        # Expand the initialized data prefix through the task stack's first byte.
        data.extend(bytes(0x1001))
        struct.pack_into('<I', data, PE_SH + 40 + 16, 0x1001)
        data[0x2200] = 1
        with self.assertRaises(ValueError):
            self.pe(data)
        data = pe_fixture()
        struct.pack_into('<I', data, PE_SH + 40 + 16, 0)
        with self.assertRaises(ValueError):
            self.pe(data)

    def test_pe_rejects_truncation_wrong_code_section_and_missing_symbols(self):
        for offset, value in ((PE_SH + 8, 0x1001), (PE_SH + 36, 0x40000040),
                              (PE_SH + 40 + 8, 0x2607f), (PE_OPT + 56, 0x2807f)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data = pe_fixture()
                struct.pack_into('<I', data, offset, value)
                self.pe(data)
        with self.assertRaises(ValueError):
            self.pe(pe_fixture()[:-1])
        text = map_fixture().replace('styleos_tasks_probe_resume', 'missing_probe_resume')
        with self.assertRaises(ValueError):
            self.pe(text=text)

    def test_proof_refuses_decoder_gap_and_function_endpoint_inside_instruction(self):
        def bad_decoder(*args):
            proof, decoded = fixture_decoder(*args)
            del decoded[ARM_BASE + 0x110]
            return proof, decoded
        with patch('tools.tasks_site.verify_integer_code', side_effect=bad_decoder):
            with self.assertRaises(ValueError):
                self.arm()
        data, _ = arm_fixture()
        struct.pack_into('<Q', data, symbol_offset('tasks_arch_switch_end') + 8,
                         ARM_BASE + 0x114)
        with self.assertRaises(ValueError):
            self.arm(data)

    def test_executable_symbols_and_functions_cannot_resolve_to_declared_padding(self):
        for address in (ARM_BASE + 0x100, ARM_BASE + 0x114):
            def padding_decoder(*args):
                proof, decoded = fixture_decoder(*args)
                decoded[address]['kind'] = 'zero-padding'
                return proof, decoded
            with self.subTest(address=address), patch(
                    'tools.tasks_site.verify_integer_code', side_effect=padding_decoder):
                with self.assertRaises(ValueError):
                    self.arm()

    def test_requires_arch_sidecars_and_regular_complete_files(self):
        binary = self.folder / 'binary'
        binary.write_bytes(b'')
        for arch in ('aarch64', 'x86_64', 'unknown'):
            with self.subTest(arch=arch), self.assertRaises(ValueError):
                describe_tasks(arch, binary)
        with self.assertRaises(ValueError):
            describe_tasks('aarch64', self.folder, raw_path=binary)


if __name__ == '__main__':
    unittest.main()
