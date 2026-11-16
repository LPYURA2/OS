"""Adversarial plan/UART/binding checks; these fixtures are not QEMU evidence."""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.test import (Checks, ownership_report, serial_memory_ok,
                        serial_ownership_ok, verified_ownership_site)


def line(label, **values):
    return ('STYLEOS ' + label + ''.join(f' {key}=0x{value:016x}'
                                        for key, value in values.items()) + '\n').encode()


def bounds(label, start, end, suffix=''):
    return f'STYLEOS {label} 0x{start:016x} 0x{end:016x}{suffix}\n'.encode()


def fixture(arch='aarch64', invalid=False, el2=False, ram_mib=128):
    base = 0x60000000 if arch == 'x86_64' else 0x40080000
    image, stack = (base, base + 0x50000), (base + 0x30000, base + 0x40000)
    handoff = (base + 0x41000, base + 0x41060) if arch == 'x86_64' else (
        stack[0] + 0x8000, stack[0] + 0x8060)
    sizes = {'diagnostic': (base + 0x5000, 12400),
             'workspace': (base + 0x10000, 65536), 'stack': (stack[0], 65536),
             'pool': (base + 0x20000, 32768), 'pool_metadata': (base + 0x28000, 4096)}
    if arch == 'x86_64':
        sizes.update(idt=(base + 0x9000, 4096), handoff=(handoff[0], 96))
        prefix = (b'STYLEOS x86_64 ExitBootServices OK\n'
                  b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        ram = base, base + 0x100000
        source = base + 0x70000, base + 0x71080
        reserved = [image, (base + 0x60000, base + 0x61000),
                    (base + 0x62000, base + 0x66000),
                    (source[0], (source[1] + 4095) & ~4095)]
        state = dict(mode=0x80000011, root=base + 0x62000, aux0=0x6a0, aux1=0xd00,
                     pat=0x0007040600070406, rflags=2, bits=40, features=3,
                     gdt_va=0x70000120, gdt_bytes=0x80,
                     idt_va=base + 0x9000, idt_bytes=4096, cs=0x38)
    else:
        sizes['vectors'] = base + 0x2000, 2048
        prefix = (b'STYLEOS AARCH64: initial EL2\n' if el2 else
                  b'STYLEOS AARCH64: initial EL1\n') + b'STYLEOS AARCH64: current EL1\n'
        ram, source = (0x40000000, 0x40000000 + (ram_mib << 20)), (0x44000000, 0x44001000)
        reserved = [(0x9000000, 0x9001000), image, source]
        state = dict(mode=0x30d00800, root=0, aux0=base + 0x2000, aux1=0x3c0,
                     pat=0, rflags=0, bits=40, features=0, gdt_va=0, gdt_bytes=0,
                     idt_va=0, idt_bytes=0, cs=0)
    prefix += (b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n' +
               f'STYLEOS HANDOFF PASS {arch}\n'.encode() + b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, spans in [('IMAGE', [image]), ('STACK', [stack]), ('HANDOFF', [handoff]),
                        ('SOURCE', [source]), ('RAM', [ram]), ('RESERVED', reserved)]:
        for start, end in spans:
            prefix += bounds('MEMMAP ' + kind, start, end)
    candidates, cursor = [], ram[0]
    for start, end in reserved:
        if end <= ram[0] or start >= ram[1]:
            continue
        if cursor < start:
            candidates.append((cursor, start))
        cursor = max(cursor, end)
    if cursor < ram[1]:
        candidates.append((cursor, ram[1]))
    for start, end in candidates:
        prefix += bounds('MEMMAP CANDIDATE', start, end)
    prefix += b'STYLEOS MEMMAP PASS\n'
    relocation = base if arch == 'x86_64' else 0
    site = dict(arch=arch, invalid=invalid, page_size=4096,
                image_start=image[0] - relocation, image_end=image[1] - relocation,
                image_size=image[1] - image[0],
                address_space='image-rva' if arch == 'x86_64' else 'identity-image-address',
                objects={name: dict(start=start - relocation, end=start + size - relocation,
                                    bytes=size, symbol='fixture_' + name)
                         for name, (start, size) in sizes.items()})
    if arch == 'x86_64':
        site['preferred_image_base'] = 0x140000000
    else:
        site.update(bss_start=base + 0x4000, bss_end=image[1])
    if invalid:
        return prefix + b'STYLEOS OWNERSHIP ERROR: required\n', site
    out = prefix + f'STYLEOS OWNERSHIP PLAN arch={arch} phase={"inherited" if arch == "x86_64" else "mmu-off"} source={"uefi" if arch == "x86_64" else "fdt"}\n'.encode()
    out += line('OWNERSHIP WORKSPACE', pa=sizes['workspace'][0], bytes=65536)
    out += line('OWNERSHIP SOURCE', pa=source[0], bytes=source[1] - source[0],
                stride=48 if arch == 'x86_64' else 0, version=1 if arch == 'x86_64' else 0)
    out += line('OWNERSHIP STATE', **state)
    common = [('IMAGE', image), ('STACK', stack), ('HANDOFF', handoff), ('SOURCE_BUFFER', source),
              ('MAP_WORKSPACE', (sizes['diagnostic'][0], sizes['diagnostic'][0] + 12400)),
              ('OWN_WORKSPACE', (sizes['workspace'][0], sizes['workspace'][0] + 65536)),
              ('BOOT_POOL', (sizes['pool'][0], sizes['pool'][0] + 32768)),
              ('POOL_METADATA', (sizes['pool_metadata'][0], sizes['pool_metadata'][0] + 4096))]
    claims = [(kind, 'BOOT_SOURCE' if kind == 'SOURCE_BUFFER' else 'IMAGE_IDENTITY', a, a, b - a)
              for kind, (a, b) in common]
    if arch == 'x86_64':
        tables = [base + off for off in (0x62000, 0x63000, 0x64000, 0x65000)]
        claims += [('GDT', 'OLD_WALK', 0x70000000, base + 0x60000, 4096),
                   ('IDT', 'OLD_WALK', state['idt_va'], state['idt_va'], 4096),
                   ('ROOT', 'REGISTER', 0, state['root'], 4096)]
        claims += [('OLD_TABLE', 'OLD_WALK', pa, pa, 4096) for pa in tables]
    else:
        claims += [('VECTORS', 'IMAGE_IDENTITY', state['aux0'], state['aux0'], 2048),
                   ('MMIO', 'FIXED_DEVICE', 0x9000000, 0x9000000, 4096)]
    for kind, proof, va, pa, size in claims:
        out += (f'STYLEOS OWNERSHIP CLAIM kind={kind} proof={proof} '.encode() +
                f'va=0x{va:016x} pa=0x{pa:016x} bytes=0x{size:016x}\n'.encode())
    if arch == 'x86_64':
        traces = [('GDT', 0x70000000, base + 0x60000,
                   [tables[1] | 3, tables[2] | 3, tables[3] | 3, (base + 0x60000) | 3])]
        traces += [('TABLE', pa, pa, [tables[1] | 3, tables[2] | 3, base | 0x83]) for pa in tables]
        for kind, va, pa, entries in traces:
            out += (f'STYLEOS OWNERSHIP WALK kind={kind}'.encode() +
                    line('', va=va, pa=pa, levels=len(entries))[8:])
            for level, entry in enumerate(entries):
                out += line('OWNERSHIP STEP', level=level, table=tables[level], entry=entry)
    out += b'STYLEOS OWNERSHIP INACTIVE paging=not-built pool=not-admitted\n'
    out += line('OWNERSHIP UNRESOLVED', flags=3 if arch == 'x86_64' else 0)
    for a, b in reserved:
        out += bounds('OWNERSHIP PROTECTED', a, b)
    for a, b in candidates:
        out += bounds('OWNERSHIP REVIEW', a, b, ' reason=no-transfer')
    out += b'STYLEOS OWNERSHIP UNCLASSIFIED outside-covered-union\n'
    out += line('OWNERSHIP TOTAL', protected=sum(b - a for a, b in reserved) // 4096,
                review=sum(b - a for a, b in candidates) // 4096, granted=0)
    out += line('OWNERSHIP STATE AFTER', **state)
    return out + b'STYLEOS OWNERSHIP PASS\nSTYLEOS KERNEL HALT\n', site


def mock_manifest(folder, arch, site, invalid=False):
    names = [('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')] if arch == 'x86_64' else [
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')]
    rows = []
    for name, key in names:
        path = folder / arch / name
        if not path.exists():
            path.write_bytes(b'fixture image bytes; parser is mocked separately')
        rows.append(dict(path=arch + '/' + name, size=path.stat().st_size, sha256=site[key]))
    manifest = dict(invalid_boot_info=False, invalid_memory_map=False, test_exception=False,
                    test_bootpool=False, test_paging=False, test_paging_fault=False,
                    test_ownership_plan=not invalid, test_ownership_plan_invalid=invalid, artifacts=rows)
    (folder / 'manifest.json').write_text(json.dumps(manifest))
    return manifest


class OwnershipOutputTests(unittest.TestCase):
    def test_all_seven_requested_profiles_validate_and_el_is_exact(self):
        jobs = [('x86_64', False, False, 128), ('aarch64', False, False, 128),
                ('aarch64', False, True, 128), ('aarch64', False, False, 256),
                ('x86_64', True, False, 128), ('aarch64', True, False, 128),
                ('aarch64', True, True, 128)]
        for arch, invalid, el2, mib in jobs:
            with self.subTest(arch=arch, invalid=invalid, el2=el2, mib=mib):
                output, site = fixture(arch, invalid, el2, mib)
                self.assertTrue(serial_ownership_ok(output, arch, site, invalid, mib, el2))
                if arch == 'aarch64':
                    self.assertFalse(serial_ownership_ok(output, arch, site, invalid, mib, not el2))

    def test_marker_only_partial_plan_timeout_and_duplicate_result_fail(self):
        output, site = fixture()
        for bad in (b'', b'STYLEOS OWNERSHIP PASS\nSTYLEOS KERNEL HALT\n',
                    output[output.index(b'STYLEOS OWNERSHIP PLAN'):],
                    output.replace(b'STYLEOS OWNERSHIP STATE AFTER', b'STYLEOS OWNERSHIP AFTER'),
                    output + b'STYLEOS OWNERSHIP PASS\n', b'timeout\n' + output,
                    output + b'QEMU timed out\n', output + b'STYLEOS OWNERSHIP ERROR: proof\n'):
            self.assertFalse(serial_ownership_ok(bad, 'aarch64', site))

    def test_old_modes_reject_any_ownership_execution(self):
        output, _site = fixture()
        prefix = output[:output.index(b'STYLEOS OWNERSHIP')]
        self.assertTrue(serial_memory_ok(prefix + b'STYLEOS KERNEL HALT\n', 'aarch64', 'normal'))
        for marker in (b'STYLEOS OWNERSHIP PASS\n', b'STYLEOS OWNERSHIP ERROR: required\n'):
            self.assertFalse(serial_memory_ok(prefix + marker + b'STYLEOS KERNEL HALT\n',
                                              'aarch64', 'normal'))

    def test_static_full_image_objects_are_required_and_relocated(self):
        output, site = fixture('x86_64')
        for key, value in [('page_size', True), ('image_size', 4096),
                            ('address_space', 'identity-image-address')]:
            self.assertFalse(serial_ownership_ok(output, 'x86_64', dict(site, **{key: value})))
        for name in site['objects']:
            changed = deepcopy(site)
            changed['objects'][name]['end'] -= 1
            self.assertFalse(serial_ownership_ok(output, 'x86_64', changed), name)
        self.assertFalse(serial_ownership_ok(output.replace(b'0x0000000060010000',
                                                             b'0x0000000140010000'), 'x86_64', site))

    def test_missing_duplicate_unknown_and_unproven_claims_fail(self):
        output, site = fixture()
        claim = next(row for row in output.splitlines(keepends=True)
                     if b'CLAIM kind=IMAGE ' in row)
        for bad in (output.replace(claim, b''), output.replace(claim, claim + claim),
                    output.replace(b'kind=IMAGE ', b'kind=UNKNOWN '),
                    output.replace(b'proof=IMAGE_IDENTITY', b'proof=UNPROVEN'),
                    output.replace(b'kind=MMIO', b'kind=GDT')):
            self.assertFalse(serial_ownership_ok(bad, 'aarch64', site))

    def test_protected_and_review_partition_are_exact_no_grants(self):
        output, site = fixture()
        protected = bounds('OWNERSHIP PROTECTED', 0x40080000, 0x400d0000)
        review = bounds('OWNERSHIP REVIEW', 0x40000000, 0x40080000, ' reason=no-transfer')
        for old, new in [(protected, b''), (protected, protected + protected),
                         (protected, bounds('OWNERSHIP PROTECTED', 0x40080000, 0x400cffff)),
                         (review, bounds('OWNERSHIP REVIEW', 0x40000000, 0x40081000, ' reason=no-transfer')),
                         (review, bounds('OWNERSHIP REVIEW', 0x30000000, 0x40080000, ' reason=no-transfer')),
                         (review, review.replace(b'no-transfer', b'free')),
                         (b'granted=0x0000000000000000', b'granted=0x0000000000000001')]:
            self.assertIn(old, output)
            self.assertFalse(serial_ownership_ok(output.replace(old, new), 'aarch64', site))

    def test_phase_source_inactive_and_unresolved_evidence_are_exact(self):
        output, site = fixture()
        for old, new in [(b'phase=mmu-off', b'phase=owned'), (b'source=fdt', b'source=uefi'),
                         (b'stride=0x0000000000000000', b'stride=0x0000000000000030'),
                         (b'paging=not-built', b'paging=active'),
                         (b'pool=not-admitted', b'pool=admitted'),
                         (b'flags=0x0000000000000000', b'flags=0x0000000000000003')]:
            self.assertFalse(serial_ownership_ok(output.replace(old, new), 'aarch64', site))
        x86, site = fixture('x86_64')
        self.assertFalse(serial_ownership_ok(x86.replace(b'flags=0x0000000000000003',
                                                          b'flags=0x0000000000000000'), 'x86_64', site))

    def test_snapshot_must_preserve_translation_and_mask_interrupts(self):
        output, site = fixture()
        after = next(row for row in output.splitlines(keepends=True)
                     if row.startswith(b'STYLEOS OWNERSHIP STATE AFTER'))
        for old, new in [(b'mode=0x0000000030d00800', b'mode=0x0000000030d00801'),
                         (b'root=0x0000000000000000', b'root=0x0000000040090000'),
                         (b'aux0=0x0000000040082000', b'aux0=0x0000000040082800'),
                         (b'aux1=0x00000000000003c0', b'aux1=0x0000000000000000')]:
            self.assertFalse(serial_ownership_ok(output.replace(after, after.replace(old, new)),
                                                 'aarch64', site))
        output, site = fixture('x86_64')
        after = next(row for row in output.splitlines(keepends=True)
                     if row.startswith(b'STYLEOS OWNERSHIP STATE AFTER'))
        arithmetic = after.replace(b'rflags=0x0000000000000002', b'rflags=0x0000000000000046')
        self.assertTrue(serial_ownership_ok(output.replace(after, arithmetic), 'x86_64', site))
        for flags in (0x202, 0x402, 0x10002):
            bad = after.replace(b'rflags=0x0000000000000002', f'rflags=0x{flags:016x}'.encode())
            self.assertFalse(serial_ownership_ok(output.replace(after, bad), 'x86_64', site))

    def test_walker_reconstructs_nonidentity_gdt_and_table_identity_closure(self):
        output, site = fixture('x86_64')
        for old, new in [(b'pa=0x0000000060060000', b'pa=0x0000000060062000'),
                         (b'entry=0x0000000060060003', b'entry=0x0000000060062003'),
                         (b'entry=0x0000000060063003', b'entry=0x0000000060064003'),
                         (b'entry=0x0000000060000083', b'entry=0x0000000070000083'),
                         (b'entry=0x0000000060000083', b'entry=0x0000000060002083'),
                         (b'entry=0x0000000060063003', b'entry=0x0800000060063003'),
                         (b'level=0x0000000000000000', b'level=0x0000000000000001'),
                         (b'features=0x0000000000000003', b'features=0x0000000000000000')]:
            self.assertIn(old, output)
            self.assertFalse(serial_ownership_ok(output.replace(old, new), 'x86_64', site))
        # Hardware may update A/D while the read-only collector runs. Only
        # those bits may vary for the same recorded table entry.
        accessed = output.replace(b'entry=0x0000000060063003',
                                  b'entry=0x0000000060063023', 1)
        self.assertTrue(serial_ownership_ok(accessed, 'x86_64', site))
        row = next(row for row in output.splitlines(keepends=True) if b'WALK kind=TABLE' in row)
        self.assertFalse(serial_ownership_ok(output.replace(row, b''), 'x86_64', site))
        row = next(row for row in output.splitlines(keepends=True) if b'CLAIM kind=OLD_TABLE' in row)
        self.assertFalse(serial_ownership_ok(output.replace(row, b''), 'x86_64', site))

    def test_controlled_refusal_cannot_publish_partial_plan_or_success(self):
        for arch in ('x86_64', 'aarch64'):
            output, site = fixture(arch, invalid=True)
            for bad in (output + b'STYLEOS OWNERSHIP PASS\n', output + b'STYLEOS KERNEL HALT\n',
                        output + b'STYLEOS OWNERSHIP PROTECTED 0x0 0x1000\n',
                        output.replace(b'ERROR: required', b'ERROR: proof'),
                        output + b'STYLEOS OWNERSHIP ERROR: required\n'):
                self.assertFalse(serial_ownership_ok(bad, arch, site, True))
            self.assertFalse(serial_ownership_ok(output, arch, dict(site, invalid=False), True))

    def test_json_plan_has_explanations_and_zero_granted(self):
        output, site = fixture('x86_64')
        self.assertTrue(serial_ownership_ok(output, 'x86_64', site))
        report = ownership_report(output, 'x86_64')
        self.assertEqual(report['phase'], 'inherited')
        self.assertEqual(report['source'], 'uefi')
        self.assertEqual(report['granted'], 0)
        self.assertEqual(report['unresolved']['flags'], 3)
        self.assertEqual(len(report['unresolved']['dependencies']), 2)
        self.assertEqual({row['reason'] for row in report['review']}, {'no-transfer'})
        self.assertEqual(len(report['walks']), 5)
        self.assertEqual(report['state_before']['root'], report['state_after']['root'])


class OwnershipBindingTests(unittest.TestCase):
    def test_sidecar_is_rederived_and_all_eight_manifest_bools_are_exact(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-binding-') as temporary:
            folder = Path(temporary)
            target = folder / 'aarch64'
            target.mkdir()
            site = dict(arch='aarch64', invalid=False, binary_sha256='elf', raw_sha256='image')
            sidecar = target / 'ownership-site.json'
            sidecar.write_text(json.dumps(site))
            baseline = mock_manifest(folder, 'aarch64', site)
            with patch('tools.test.describe_ownership', return_value=site.copy()) as derive:
                self.assertEqual(verified_ownership_site(folder, 'aarch64'), site)
                derive.assert_called_once_with('aarch64', target / 'kernel.elf',
                                               raw_path=target / 'styleos.bin', invalid=False)
                sidecar.write_text(json.dumps(dict(site, raw_sha256='edited')))
                with self.assertRaisesRegex(ValueError, 'independently checked'):
                    verified_ownership_site(folder, 'aarch64')
                sidecar.write_text(json.dumps(site))
                for key in ('invalid_boot_info', 'invalid_memory_map', 'test_exception',
                            'test_bootpool', 'test_paging', 'test_paging_fault',
                            'test_ownership_plan', 'test_ownership_plan_invalid'):
                    for value in (not baseline[key], int(baseline[key])):
                        changed = deepcopy(baseline)
                        changed[key] = value
                        (folder / 'manifest.json').write_text(json.dumps(changed))
                        with self.assertRaisesRegex(ValueError, 'manifest'):
                            verified_ownership_site(folder, 'aarch64')

    def test_manifest_rows_hashes_sizes_and_booted_pe_disk_are_bound(self):
        for arch in ('x86_64', 'aarch64'):
            with self.subTest(arch=arch), tempfile.TemporaryDirectory() as temporary:
                folder = Path(temporary)
                target = folder / arch
                target.mkdir()
                derived = dict(arch=arch, invalid=True, binary_sha256='binary')
                if arch == 'x86_64':
                    (target / 'styleos.img').write_bytes(b'fixture disk bytes')
                    derived['map_sha256'] = 'map'
                    site = dict(derived, disk_sha256=hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest())
                else:
                    derived['raw_sha256'] = 'image'
                    site = derived.copy()
                (target / 'ownership-site.json').write_text(json.dumps(site))
                baseline = mock_manifest(folder, arch, site, invalid=True)
                with patch('tools.test.describe_ownership', return_value=derived.copy()):
                    self.assertEqual(verified_ownership_site(folder, arch, True), site)
                    for key, value in [('size', True), ('size', 1), ('sha256', 'edited'),
                                       ('path', arch + '/other.bin')]:
                        changed = deepcopy(baseline)
                        changed['artifacts'][0][key] = value
                        (folder / 'manifest.json').write_text(json.dumps(changed))
                        with self.assertRaisesRegex(ValueError, 'manifest'):
                            verified_ownership_site(folder, arch, True)
                    changed = deepcopy(baseline)
                    changed['artifacts'].append(changed['artifacts'][0].copy())
                    (folder / 'manifest.json').write_text(json.dumps(changed))
                    with self.assertRaises(ValueError):
                        verified_ownership_site(folder, arch, True)
                    if arch == 'x86_64':
                        (folder / 'manifest.json').write_text(json.dumps(baseline))
                        (target / 'styleos.img').write_bytes(b'changed disk bytes')
                        with self.assertRaises(ValueError):
                            verified_ownership_site(folder, arch, True)


class OwnershipInventoryTests(unittest.TestCase):
    def test_full_inventory_adds_exactly_seven_jobs_and_two_builds(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-inventory-') as temporary:
            runner = Checks(Path(temporary))
            jobs = []
            def inspect(job):
                jobs.append(job)
                return job[0], 'PASS', 'inventory fixture', ''
            with (patch.object(runner, 'build', return_value=True) as builds,
                  patch.object(runner, 'run_qemu', side_effect=inspect),
                  patch.object(runner, 'record'), patch('tools.test.shutil.which', return_value='/fake/qemu'),
                  patch('tools.test.firmware_path', return_value=Path('/fake/firmware'))):
                runner.qemu(25, ('x86_64', 'aarch64'))
            added = [job for job in jobs if job[5] in ('ownership', 'ownership-invalid')]
            self.assertEqual(len(added), 7)
            self.assertEqual({(j[1], j[5], j[6], j[7]) for j in added}, {
                ('x86_64', 'ownership', False, 128), ('x86_64', 'ownership-invalid', False, 128),
                ('aarch64', 'ownership', False, 128), ('aarch64', 'ownership', True, 128),
                ('aarch64', 'ownership', False, 256), ('aarch64', 'ownership-invalid', False, 128),
                ('aarch64', 'ownership-invalid', True, 128)})
            self.assertEqual(builds.call_count, 10)
            for job in added:
                self.assertEqual(job[3], runner.output / job[5])

    def test_arm_route_has_five_ownership_jobs_without_firmware_or_x86_builds(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-arm-') as temporary:
            runner = Checks(Path(temporary))
            jobs = []
            def inspect(job):
                jobs.append(job)
                return job[0], 'PASS', 'inventory fixture', ''
            with (patch.object(runner, 'build', return_value=True) as builds,
                  patch.object(runner, 'run_qemu', side_effect=inspect),
                  patch.object(runner, 'record'), patch('tools.test.shutil.which', return_value='/fake/qemu'),
                  patch('tools.test.firmware_path') as firmware):
                runner.qemu(25, ('aarch64',))
            firmware.assert_not_called()
            self.assertTrue(all(call.args[3] == 'aarch64' for call in builds.call_args_list))
            self.assertTrue(all(job[1] == 'aarch64' and job[4] is None for job in jobs))
            self.assertEqual(len([job for job in jobs if job[5].startswith('ownership')]), 5)


@unittest.skipUnless(os.name == 'posix', 'POSIX pipe/signal lifecycle')
class OwnershipProcessTests(unittest.TestCase):
    def run_fake(self, folder, output, mode, after='while True:\n    time.sleep(1)', timeout=1.5,
                 arch='aarch64', idt=True):
        fake = folder / 'fake-qemu'
        fake.write_text(f'#!{sys.executable}\nimport os,signal,time\nsignal.alarm(5)\n' +
                        f'os.write(1, {output!r})\n' + after + '\n')
        fake.chmod(0o700)
        (folder / 'manifest.json').write_text('{}')
        runner = Checks(folder)
        firmware = None
        if arch == 'x86_64':
            code, variables = folder / 'code.fd', folder / 'vars.fd'
            code.write_bytes(b'fixture firmware, not executed')
            variables.write_bytes(b'fixture variables')
            firmware = code, variables
        job = ('fixture-ownership', arch, str(fake), folder, firmware, mode, False, 128, timeout)
        with (patch('tools.test.verified_ownership_site', return_value={}),
              patch('tools.test.serial_ownership_ok', return_value=True) as classify,
              patch('tools.test.serial_idt_ready', return_value=idt)):
            result = runner.run_qemu(job)
        summary = json.loads((runner.logs / 'fixture-ownership.json').read_text())
        return result, summary, classify

    def test_positive_and_refusal_require_stable_halt_then_intentional_stop(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-process-') as temporary:
            folder = Path(temporary)
            for mode in ('ownership', 'ownership-invalid'):
                output, _site = fixture(invalid=mode == 'ownership-invalid')
                result, summary, classify = self.run_fake(folder, output, mode)
                self.assertEqual(result[1], 'PASS', result)
                self.assertEqual(summary['status'], 'PASS')
                self.assertTrue(summary['requested_stop'])
                self.assertFalse(summary['natural_exit'])
                self.assertTrue(summary['settled'])
                self.assertEqual(summary['uart_sha256'], hashlib.sha256(output).hexdigest())
                if mode == 'ownership':
                    self.assertEqual(summary['ownership_plan']['granted'], 0)
                    self.assertIsNone(summary['refusal'])
                else:
                    self.assertIsNone(summary['ownership_plan'])
                    self.assertEqual(summary['refusal'], 'required')
                classify.assert_called_once()

    def test_json_status_is_after_shared_idt_guard(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-idt-') as temporary:
            folder = Path(temporary)
            for mode in ('ownership', 'ownership-invalid'):
                output, _site = fixture('x86_64', mode == 'ownership-invalid')
                result, summary, _classify = self.run_fake(folder, output, mode, arch='x86_64', idt=False)
                self.assertEqual(result[1], 'FAIL')
                self.assertEqual(summary['status'], 'FAIL')
                self.assertIsNone(summary['ownership_plan'])
                self.assertIsNone(summary['refusal'])

    def test_timeout_and_natural_exit_never_reach_classifier_or_publish_plan(self):
        with tempfile.TemporaryDirectory(prefix='styleos-ownership-terminal-') as temporary:
            folder = Path(temporary)
            for mode, marker in [('ownership', b'STYLEOS OWNERSHIP PASS\n'),
                                 ('ownership-invalid', b'STYLEOS OWNERSHIP ERROR: required\n')]:
                result, summary, classify = self.run_fake(folder, marker, mode, timeout=0.1)
                self.assertEqual(result[1], 'FAIL')
                self.assertEqual(summary['status'], 'FAIL')
                self.assertIsNone(summary['ownership_plan'])
                self.assertIsNone(summary['refusal'])
                classify.assert_not_called()
                output, _site = fixture(invalid=mode == 'ownership-invalid')
                result, summary, classify = self.run_fake(folder, output, mode, after='os._exit(0)')
                self.assertEqual(result[1], 'FAIL')
                self.assertIn('exited/reset', result[2])
                self.assertTrue(summary['natural_exit'])
                self.assertIsNone(summary['ownership_plan'])
                classify.assert_not_called()


if __name__ == '__main__':
    unittest.main()
