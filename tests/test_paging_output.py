"""Adversarial UART/proof fixtures; these are host checks, not boot results."""
from pathlib import Path
from copy import deepcopy
import hashlib
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.test import (Checks, serial_memory_ok, serial_paging_ok,
                        verified_paging_site)

ALIAS, PROBE = 0x400000000000, 0x400000001000


def line(label, **values):
    return ('STYLEOS ' + label + ''.join(f' {key}=0x{value:016x}'
                                        for key, value in values.items()) + '\n').encode()


def bounds(label, start, end):
    return f'STYLEOS {label} 0x{start:016x} 0x{end:016x}\n'.encode()


def mock_manifest(folder, arch, site, fault):
    """Metadata fixtures for the runner; actual ELF/PE checks have byte tests."""
    names = [('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')] if arch == 'x86_64' else [
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')]
    rows = []
    for name, key in names:
        path = folder / arch / name
        if not path.exists():
            path.write_bytes(b'fixture data, image parser mocked separately')
        rows.append(dict(path=arch + '/' + name, size=path.stat().st_size, sha256=site[key]))
    manifest = dict(invalid_boot_info=False, invalid_memory_map=False,
                    test_exception=False, test_bootpool=False,
                    test_paging=not fault, test_paging_fault=fault, artifacts=rows)
    (folder / 'manifest.json').write_text(json.dumps(manifest))
    return manifest


def fixture(arch='aarch64', fault=False, el2=False):
    base = 0x60000000 if arch == 'x86_64' else 0x40080000
    image = base, base + 0x80000
    tables, control, data = base + 0x10000, base + 0x30000, base + 0x31000
    pool, pool_metadata = base + 0x40000, base + 0x48000
    workspace = base + 0x5000, base + 0x8000
    if arch == 'x86_64':
        prefix = (b'STYLEOS x86_64 ExitBootServices OK\n'
                  b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        ram = (base, base + 0xc0000)
        source = base + 0xa0000, base + 0xa1000
        reserved = [image, (base + 0x90000, base + 0x91000), source]
    else:
        prefix = (b'STYLEOS AARCH64: initial EL2\n' if el2 else
                  b'STYLEOS AARCH64: initial EL1\n')
        prefix += b'STYLEOS AARCH64: current EL1\n'
        ram = 0x40000000, 0x48000000
        source = 0x44000000, 0x44001000
        reserved = [(0x9000000, 0x9001000), image, source]
    prefix += (b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n' +
               f'STYLEOS HANDOFF PASS {arch}\n'.encode() +
               b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, spans in [('IMAGE', [image]),
                        ('STACK', [(base + 0x60000, base + 0x70000)]),
                        ('HANDOFF', [(base + 0x71000, base + 0x71060)]),
                        ('SOURCE', [source]), ('RAM', [ram]), ('RESERVED', reserved)]:
        for start, end in spans:
            prefix += bounds('MEMMAP ' + kind, start, end)
    cursor = ram[0]
    for start, end in reserved:
        if end <= ram[0] or start >= ram[1]:
            continue
        if cursor < start:
            prefix += bounds('MEMMAP CANDIDATE', cursor, start)
        cursor = max(cursor, end)
    if cursor < ram[1]:
        prefix += bounds('MEMMAP CANDIDATE', cursor, ram[1])
    prefix += b'STYLEOS MEMMAP PASS\n'
    out = prefix
    for label, start, end in [('TABLES', tables, tables + 0x20000),
                              ('CONTROL', control, control + 4096),
                              ('DATA', data, data + 4096),
                              ('WORKSPACE', *workspace)]:
        out += bounds('PAGING ' + label, start, end)
    if arch == 'x86_64':
        out += line('PAGING X86 BEFORE', cr0=0x80000011, cr3=base + 0xb0000,
                    cr4=0x6a0, efer=0xd00, pat=0x0007040600070406,
                    rflags=2, bits=40)
        # A deliberately nonidentity GDT VA proves the parser never derives
        # its PA by assuming all firmware pointers are physical addresses.
        out += line('PAGING X86 GDT', va=0x70000120, pa=base + 0x90120,
                    bytes=0x80, cs=0x38, descriptor=0x00af9b000000ffff)
        out += line('PAGING X86 IDT', va=base + 0x8000, bytes=4096)
        platform = (0x70000000, base + 0x90000, 4096, 0)
    else:
        out += line('PAGING ARM BEFORE', mmfr0=2, sctlr=0x30d00800,
                    vbar=base + 0x2000, daif=0x3c0)
        platform = (0x9000000, 0x9000000, 4096, 5)
    mappings = [(base, base, image[1] - base, 3), (ALIAS, data, 4096, 1), platform]
    for va, pa, size, flags in mappings:
        out += line('PAGING MAP', va=va, pa=pa, bytes=size, flags=flags)
    out += line('PAGING ROOT', pa=tables, pages=8 if arch == 'x86_64' else 9)
    if arch == 'x86_64':
        out += line('PAGING X86 AFTER', cr0=0x80010011, cr3=tables,
                    cr4=0x6a0, efer=0xd00)
    else:
        tcr = 16 | (3 << 12) | (16 << 16) | (1 << 23) | (2 << 30) | (2 << 32)
        out += line('PAGING ARM AFTER', ttbr0=tables, ttbr1=0, tcr=tcr,
                    mair=0x44, sctlr=0x30d00801, vbar=base + 0x2000)
    for va, pa, size, flags in mappings:
        for offset in range(0, size, 4096):
            out += line('PAGING PTE', va=va + offset, pa=pa + offset, flags=flags)
    out += line('PAGING ALIAS', va=ALIAS, pa=data, before=0x123456789abcdef0,
                after=0xfedcba9876543210)
    objects = {}
    relocation = base if arch == 'x86_64' else 0
    for name, address, size in [('tables', tables, 131072), ('control', control, 4096),
                                ('data', data, 4096), ('pool', pool, 32768),
                                ('pool_metadata', pool_metadata, 4096)]:
        objects[name] = dict(start=address - relocation, end=address + size - relocation,
                             bytes=size)
    site = dict(arch=arch, fault=fault, page_size=4096, table_pages=32,
                alias=ALIAS, probe=PROBE, image_start=base - relocation,
                image_end=image[1] - relocation, image_size=image[1] - base,
                objects={name: objects[name] for name in ('tables', 'control', 'data')},
                boot_pool={name: objects[name] for name in ('pool', 'pool_metadata')},
                address_space='image-rva' if arch == 'x86_64' else 'identity-image-address')
    if arch == 'x86_64':
        site['preferred_image_base'] = 0x140000000
    if not fault:
        return out + b'STYLEOS PAGING PASS\nSTYLEOS KERNEL HALT\n', site
    pc = base + 0x1000
    site['fault_site'] = dict(symbol='styleos_paging_fault',
                              instruction_hex='488b01' if arch == 'x86_64' else '010040f9',
                              **({'rva': 0x1000} if arch == 'x86_64' else {'pc': pc}))
    out += line('PAGING FAULT READY', va=PROBE, pc=pc)
    if arch == 'x86_64':
        out += (f'STYLEOS EXCEPTION arch=x86_64 kind=sync vector=14 reason=page-fault '
                f'pc=0x{pc:016x} error=0x0000000000000000 cr2=0x{PROBE:016x}\n').encode()
    else:
        out += (f'STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x25 reason=translation-fault '
                f'pc=0x{pc:016x} esr=0x0000000096000007 elr=0x{pc:016x} '
                f'spsr=0x00000000600003c5 far=0x{PROBE:016x}\n').encode()
    return out + b'STYLEOS EMERGENCY HALT\n', site


class PagingOutputTests(unittest.TestCase):
    def test_all_six_profiles_and_correct_initial_el(self):
        for arch, el2 in [('x86_64', False), ('aarch64', False), ('aarch64', True)]:
            for fault in (False, True):
                with self.subTest(arch=arch, el2=el2, fault=fault):
                    output, site = fixture(arch, fault, el2)
                    self.assertTrue(serial_paging_ok(output, arch, site, fault, el2=el2))
                    if arch == 'aarch64':
                        self.assertFalse(serial_paging_ok(output, arch, site, fault, el2=not el2))

    def test_empty_marker_only_missing_prefix_and_timeouts_fail(self):
        output, site = fixture()
        for bad in (b'', b'STYLEOS PAGING PASS\nSTYLEOS KERNEL HALT\n',
                    output[output.index(b'STYLEOS PAGING TABLES'):],
                    b'timeout\n' + output, output + b'QEMU timed out\n',
                    output + b'STYLEOS PAGING ERROR: proof\n'):
            self.assertFalse(serial_paging_ok(bad, 'aarch64', site))

    def test_old_profiles_reject_paging_execution(self):
        output, _site = fixture()
        prefix = output[:output.index(b'STYLEOS PAGING')]
        self.assertTrue(serial_memory_ok(prefix + b'STYLEOS KERNEL HALT\n', 'aarch64', 'normal'))
        for marker in (b'STYLEOS PAGING PASS\n', b'STYLEOS PAGING ROOT pa=0x0 pages=0x1\n'):
            self.assertFalse(serial_memory_ok(prefix + marker + b'STYLEOS KERNEL HALT\n',
                                              'aarch64', 'normal'))

    def test_complete_binary_bounds_and_excluded_workspace_are_required(self):
        output, site = fixture()
        for old, new in [(bounds('PAGING TABLES', 0x40090000, 0x400b0000),
                           bounds('PAGING TABLES', 0x40090000, 0x400affff)),
                         (bounds('PAGING WORKSPACE', 0x40085000, 0x40088000),
                           bounds('PAGING WORKSPACE', 0x40090000, 0x40091000)),
                         (bounds('PAGING CONTROL', 0x400b0000, 0x400b1000),
                           bounds('PAGING CONTROL', 0x40090000, 0x40091000))]:
            self.assertIn(old, output)
            self.assertFalse(serial_paging_ok(output.replace(old, new), 'aarch64', site))
        for key, value in [('page_size', True), ('image_size', 4096), ('table_pages', 33),
                            ('alias', 0x400000000001), ('probe', ALIAS)]:
            changed = dict(site, **{key: value})
            self.assertFalse(serial_paging_ok(output, 'aarch64', changed))
        changed = deepcopy(site)
        changed['objects']['tables']['end'] -= 4096
        self.assertFalse(serial_paging_ok(output, 'aarch64', changed))

    def test_exact_map_set_rights_types_and_unmapped_probe(self):
        output, site = fixture()
        rows = [line('PAGING MAP', va=0x40080000, pa=0x40080000, bytes=0x80000, flags=3),
                line('PAGING MAP', va=ALIAS, pa=0x400b1000, bytes=4096, flags=1),
                line('PAGING MAP', va=0x9000000, pa=0x9000000, bytes=4096, flags=5)]
        changes = [(rows[0], rows[0].replace(b'0000000000080000', b'0000000008000000')),
                   (rows[1], rows[1].replace(b'flags=0x0000000000000001', b'flags=0x0000000000000003')),
                   (rows[2], rows[2].replace(b'flags=0x0000000000000005', b'flags=0x0000000000000001')),
                   (rows[2], b''), (rows[1], rows[1] + rows[1]),
                   (rows[1], rows[1] + line('PAGING MAP', va=PROBE, pa=0x400b1000,
                                            bytes=4096, flags=1))]
        for old, new in changes:
            self.assertIn(old, output)
            self.assertFalse(serial_paging_ok(output.replace(old, new), 'aarch64', site))

    def test_owned_root_readback_and_every_live_leaf_are_required(self):
        output, site = fixture()
        pte = line('PAGING PTE', va=ALIAS, pa=0x400b1000, flags=1)
        for bad in (output.replace(pte, b''), output.replace(pte, pte + pte),
                    output.replace(pte, pte.replace(b'flags=0x0000000000000001', b'flags=0x0000000000000003')),
                    output.replace(b'ROOT pa=0x0000000040090000', b'ROOT pa=0x00000000400b0000'),
                    output.replace(b'pages=0x0000000000000009', b'pages=0x0000000000000021'),
                    output.replace(b'pages=0x0000000000000009', b'pages=0x0000000000000001'),
                    output.replace(b'ttbr0=0x0000000040090000', b'ttbr0=0x0000000040091000'),
                    output.replace(b'ttbr1=0x0000000000000000', b'ttbr1=0x0000000040090000'),
                    output.replace(b'sctlr=0x0000000030d00801', b'sctlr=0x0000000030d00800'),
                    output.replace(b'mair=0x0000000000000044', b'mair=0x00000000000000ff')):
            self.assertFalse(serial_paging_ok(bad, 'aarch64', site))

    def test_actual_alias_rw_and_exact_terminal_order(self):
        output, site = fixture()
        for bad in (output.replace(b'after=0xfedcba9876543210', b'after=0x123456789abcdef0'),
                    output.replace(b'before=0x123456789abcdef0', b'before=0x0000000000000000'),
                    output.replace(b'STYLEOS PAGING PASS\nSTYLEOS KERNEL HALT\n',
                                   b'STYLEOS KERNEL HALT\nSTYLEOS PAGING PASS\n'),
                    output + b'STYLEOS PAGING PASS\n', output + b'STYLEOS EMERGENCY HALT\n'):
            self.assertFalse(serial_paging_ok(bad, 'aarch64', site))

    def test_x86_requires_relocated_root_gdt_and_supported_mode(self):
        output, site = fixture('x86_64')
        for old, new in [(b'cr3=0x0000000060010000', b'cr3=0x0000000140010000'),
                         (b'cr3=0x00000000600b0000', b'cr3=0x00000000600b0008'),
                         (b'cr3=0x00000000600b0000', b'cr3=0x00000100600b0000'),
                         (b'cr4=0x00000000000006a0', b'cr4=0x00000000000206a0'),
                         (b'rflags=0x0000000000000002', b'rflags=0x0000000000000202'),
                         (b'pat=0x0007040600070406', b'pat=0x0007040600070400'),
                         (b'bits=0x0000000000000028', b'bits=0x000000000000001f'),
                         (b'pa=0x0000000060090120', b'pa=0x0000000070000120'),
                         (b'va=0x0000000070000120', b'va=0x0000000060000120'),
                         (b'pa=0x0000000060090000', b'pa=0x0000000060010000'),
                         (b'bytes=0x0000000000000080', b'bytes=0x0000000000000038')]:
            self.assertIn(old, output)
            self.assertFalse(serial_paging_ok(output.replace(old, new), 'x86_64', site))
        without_gdt = output.replace(line('PAGING MAP', va=0x70000000,
                                           pa=0x60090000, bytes=4096, flags=0), b'')
        self.assertFalse(serial_paging_ok(without_gdt, 'x86_64', site))

    def test_x86_fault_is_a_single_supervisor_nonpresent_read_at_built_pc(self):
        output, site = fixture('x86_64', True)
        for old, new in [(b'vector=14', b'vector=13'), (b'reason=page-fault', b'reason=general-protection'),
                         (b'error=0x0000000000000000', b'error=0x0000000000000001'),
                         (b'error=0x0000000000000000', b'error=0x0000000000000002'),
                         (b'error=0x0000000000000000', b'error=0x0000000000000004'),
                         (b'error=0x0000000000000000', b'error=0x0000000000000008'),
                         (b'cr2=0x0000400000001000', b'cr2=0x0000400000000000'),
                         (b'pc=0x0000000060001000', b'pc=0x0000000140001000')]:
            self.assertFalse(serial_paging_ok(output.replace(old, new), 'x86_64', site, True))

    def test_only_unaccessed_cs_access_byte_page_can_be_writable(self):
        output, site = fixture('x86_64')
        readonly_map = line('PAGING MAP', va=0x70000000, pa=0x60090000, bytes=4096, flags=0)
        readonly_pte = line('PAGING PTE', va=0x70000000, pa=0x60090000, flags=0)
        pending = output.replace(b'descriptor=0x00af9b000000ffff', b'descriptor=0x00af9a000000ffff')
        self.assertFalse(serial_paging_ok(pending, 'x86_64', site))
        writable = pending.replace(readonly_map, readonly_map.replace(b'flags=0x0000000000000000',
                                                                      b'flags=0x0000000000000001'))
        writable = writable.replace(readonly_pte, readonly_pte.replace(b'flags=0x0000000000000000',
                                                                      b'flags=0x0000000000000001'))
        self.assertTrue(serial_paging_ok(writable, 'x86_64', site))
        self.assertFalse(serial_paging_ok(writable.replace(b'descriptor=0x00af9a000000ffff',
                                                           b'descriptor=0x00af9b000000ffff'),
                                          'x86_64', site))
        for descriptor in (0x00af92000000ffff, 0x00affb000000ffff,
                           0x00cf9b000000ffff, 0x008f9b000000ffff):
            bad = output.replace(b'descriptor=0x00af9b000000ffff',
                                  f'descriptor=0x{descriptor:016x}'.encode())
            self.assertFalse(serial_paging_ok(bad, 'x86_64', site))
        # Preserve the normal memory-validator's exact candidate partition,
        # but remove the external GDT page from reservations.
        unreserved = writable.replace(bounds('MEMMAP RESERVED', 0x60090000, 0x60091000), b'')
        unreserved = unreserved.replace(bounds('MEMMAP CANDIDATE', 0x60080000, 0x60090000) +
                                         bounds('MEMMAP CANDIDATE', 0x60091000, 0x600a0000),
                                         bounds('MEMMAP CANDIDATE', 0x60080000, 0x600a0000))
        prefix = unreserved[:unreserved.index(b'STYLEOS PAGING')]
        self.assertTrue(serial_memory_ok(prefix, 'x86_64', 'normal', terminal=False))
        self.assertFalse(serial_paging_ok(unreserved, 'x86_64', site))

    def test_arm_fault_is_current_el_read_translation_with_valid_far(self):
        output, site = fixture('aarch64', True)
        for syndrome in (0x92000007, 0x94000007, 0x96000000, 0x96000009,
                         0x96000047, 0x96000087, 0x96000107, 0x96000207, 0x96000407):
            bad = output.replace(b'esr=0x0000000096000007', f'esr=0x{syndrome:016x}'.encode())
            self.assertFalse(serial_paging_ok(bad, 'aarch64', site, True))
        for old, new in [(b'class=0x25', b'class=0x24'), (b'far=0x0000400000001000', b'far=absent'),
                         (b'elr=0x0000000040081000', b'elr=0x0000000040081004'),
                         (b'spsr=0x00000000600003c5', b'spsr=0x00000000600003c9')]:
            self.assertFalse(serial_paging_ok(output.replace(old, new), 'aarch64', site, True))

    def test_fault_cannot_continue_or_hide_duplicate_and_wrong_fault_site(self):
        output, site = fixture('aarch64', True)
        record = next(row for row in output.splitlines(keepends=True) if row.startswith(b'STYLEOS EXCEPTION'))
        for bad in (output + b'STYLEOS KERNEL HALT\n', output + b'STYLEOS PAGING PASS\n',
                    output + b'STYLEOS EMERGENCY HALT\n', output + record,
                    output.replace(b'STYLEOS PAGING FAULT READY', b'STYLEOS PAGING READY'),
                    output.replace(record, b'')):
            self.assertFalse(serial_paging_ok(bad, 'aarch64', site, True))
        changed = deepcopy(site)
        changed['fault_site']['instruction_hex'] = '002420d4'
        self.assertFalse(serial_paging_ok(output, 'aarch64', changed, True))


class PagingBindingTests(unittest.TestCase):
    def test_proof_is_rederived_instead_of_trusting_edited_json(self):
        with tempfile.TemporaryDirectory(prefix='styleos-paging-proof-') as temporary:
            folder = Path(temporary)
            target = folder / 'aarch64'
            target.mkdir()
            site = dict(arch='aarch64', binary_sha256='elf', raw_sha256='image', fault=True)
            sidecar = target / 'paging-site.json'
            sidecar.write_text(json.dumps(site))
            mock_manifest(folder, 'aarch64', site, True)
            with patch('tools.test.describe_paging', return_value=site.copy()) as derive:
                self.assertEqual(verified_paging_site(folder, 'aarch64', True), site)
                derive.assert_called_once_with('aarch64', target / 'kernel.elf',
                                               raw_path=target / 'styleos.bin', fault=True)
            sidecar.write_text(json.dumps(dict(site, raw_sha256='edited')))
            with patch('tools.test.describe_paging', return_value=site):
                with self.assertRaisesRegex(ValueError, 'independently checked'):
                    verified_paging_site(folder, 'aarch64', True)

    def test_pe_proof_also_binds_actual_booted_disk(self):
        with tempfile.TemporaryDirectory(prefix='styleos-paging-disk-') as temporary:
            folder = Path(temporary)
            target = folder / 'x86_64'
            target.mkdir()
            disk = target / 'styleos.img'
            disk.write_bytes(b'fixture disk bytes, not an OS image')
            derived = dict(arch='x86_64', binary_sha256='pe', map_sha256='map', fault=False)
            site = dict(derived, disk_sha256=hashlib.sha256(disk.read_bytes()).hexdigest())
            (target / 'paging-site.json').write_text(json.dumps(site))
            mock_manifest(folder, 'x86_64', site, False)
            with patch('tools.test.describe_paging', return_value=derived.copy()):
                self.assertEqual(verified_paging_site(folder, 'x86_64'), site)
            disk.write_bytes(b'changed disk bytes')
            with patch('tools.test.describe_paging', return_value=derived.copy()):
                with self.assertRaises(ValueError):
                    verified_paging_site(folder, 'x86_64')

    def test_manifest_mode_rows_size_and_hash_are_all_bound(self):
        with tempfile.TemporaryDirectory(prefix='styleos-paging-manifest-') as temporary:
            folder = Path(temporary)
            target = folder / 'aarch64'
            target.mkdir()
            site = dict(arch='aarch64', binary_sha256='elf', raw_sha256='image', fault=False)
            (target / 'paging-site.json').write_text(json.dumps(site))
            baseline = mock_manifest(folder, 'aarch64', site, False)
            bad = []
            for key, value in [('test_paging', 1), ('test_paging_fault', True),
                               ('test_bootpool', True), ('invalid_memory_map', True)]:
                changed = deepcopy(baseline)
                changed[key] = value
                bad.append(changed)
            for key, value in [('size', 1), ('size', True), ('sha256', 'wrong'),
                               ('path', 'aarch64/other.bin')]:
                changed = deepcopy(baseline)
                changed['artifacts'][0][key] = value
                bad.append(changed)
            changed = deepcopy(baseline)
            changed['artifacts'].append(changed['artifacts'][0].copy())
            bad.append(changed)
            with patch('tools.test.describe_paging', return_value=site.copy()):
                self.assertEqual(verified_paging_site(folder, 'aarch64'), site)
                for changed in bad:
                    (folder / 'manifest.json').write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, 'manifest'):
                        verified_paging_site(folder, 'aarch64')


@unittest.skipUnless(os.name == 'posix', 'POSIX pipe/signal lifecycle')
class PagingTimeoutTests(unittest.TestCase):
    def test_paging_json_and_final_result_include_shared_idt_guard(self):
        with tempfile.TemporaryDirectory(prefix='styleos-paging-json-') as temporary:
            folder = Path(temporary)
            code, var = folder / 'code.fd', folder / 'vars.fd'
            code.write_bytes(b'fixture firmware, not executed')
            var.write_bytes(b'fixture variables')
            (folder / 'manifest.json').write_text('{}')
            fake = folder / 'fake-qemu'
            fake.write_text(f'#!{sys.executable}\n'
                            'import os,signal,time\nsignal.alarm(5)\n'
                            'os.write(1,b"STYLEOS KERNEL HALT\\n")\n'
                            'while True:\n    time.sleep(1)\n')
            fake.chmod(0o700)
            runner = Checks(folder)
            job = ('fixture-json', 'x86_64', str(fake), folder, (code, var),
                   'paging', False, 128, 1.5)
            # Isolate the shared guard: an earlier classifier's success must
            # not be serialized as PASS before the final IDT check runs.
            with (patch('tools.test.verified_paging_site', return_value={}),
                  patch('tools.test.serial_paging_ok', return_value=True),
                  patch('tools.test.serial_idt_ready', return_value=False)):
                result = runner.run_qemu(job)
            self.assertEqual(result[1], 'FAIL')
            summary = json.loads((runner.logs / 'fixture-json.json').read_text())
            self.assertEqual(summary['status'], 'FAIL')
            self.assertTrue(summary['requested_stop'])
            self.assertFalse(summary['natural_exit'])

    def test_both_paging_pass_and_fault_ready_alone_cannot_survive_timeout(self):
        with tempfile.TemporaryDirectory(prefix='styleos-paging-timeout-') as temporary:
            folder = Path(temporary)
            for mode, marker in [('paging', 'STYLEOS PAGING PASS'),
                                 ('paging-fault', 'STYLEOS PAGING FAULT READY')]:
                fake = folder / ('fake-' + mode)
                fake.write_text(f'#!{sys.executable}\n'
                                'import os,signal,time\nsignal.alarm(5)\n' +
                                f'os.write(1,{(marker + chr(10)).encode()!r})\n' +
                                'while True:\n    time.sleep(1)\n')
                fake.chmod(0o700)
                runner = Checks(folder)
                job = ('fixture-' + mode, 'aarch64', str(fake), folder, None,
                       mode, False, 128, 0.2)
                with patch('tools.test.serial_paging_ok', return_value=True) as classify:
                    result = runner.run_qemu(job)
                self.assertEqual(result[1], 'FAIL', result)
                self.assertIn('timeout', result[2])
                classify.assert_not_called()


if __name__ == '__main__':
    unittest.main()
