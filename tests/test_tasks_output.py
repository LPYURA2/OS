"""Adversarial task/UART/binding fixtures; these do not prove a QEMU switch."""
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
from tools.test import (Checks, _TASKS_CODE, _tasks_sources, serial_memory_ok, serial_tasks_ok,
                        tasks_report, verified_tasks_site)


def line(label, **values):
    return ('STYLEOS ' + label + ''.join(f' {key}=0x{value:016x}'
                                        for key, value in values.items()) + '\n').encode()


def bounds(label, start, end):
    return f'STYLEOS {label} 0x{start:016x} 0x{end:016x}\n'.encode()


def fixture(arch='aarch64', el2=False):
    base = 0x60000000 if arch == 'x86_64' else 0x40080000
    image, stack = (base, base + 0x50000), (base + 0x30000, base + 0x40000)
    handoff = (base + 0x41000, base + 0x41060) if arch == 'x86_64' else (
        stack[0] + 0x8000, stack[0] + 0x8060)
    objects = dict(stack_a=(base + 0x10000, 16384), stack_b=(base + 0x14000, 16384),
                   control=(base + 0x18000, 4096), diagnostic=(base + 0x5000, 12400),
                   stack=(stack[0], 65536), pool=(base + 0x20000, 32768),
                   pool_metadata=(base + 0x28000, 4096))
    state = dict(mode=0, root=0, control0=0, control1=0, flags=0, cs=0,
                 idt_base=0, idt_bytes=0, gdt_base=0, gdt_bytes=0, pat=0,
                 pa_bits=40, gdt_pages=0, gdt_pa0=0, gdt_pa1=0)
    if arch == 'x86_64':
        objects.update(idt=(base + 0x9000, 4096), handoff=(handoff[0], 96))
        prefix = (b'STYLEOS x86_64 ExitBootServices OK\n'
                  b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        ram, source = (base, base + 0x100000), (base + 0x70000, base + 0x71080)
        reserved = [image, (base + 0x60000, base + 0x61000),
                    (base + 0x62000, base + 0x63000),
                    (source[0], (source[1] + 4095) & ~4095)]
        state.update(mode=0x80000011, root=base + 0x62000, control0=0x6a0,
                     control1=0xd00, flags=2, cs=0x38, idt_base=base + 0x9000,
                     idt_bytes=4096, gdt_base=0x70000120, gdt_bytes=0x80,
                     pat=0x0007040600070406, gdt_pages=1, gdt_pa0=base + 0x60000)
    else:
        objects['vectors'] = base + 0x2000, 2048
        prefix = (b'STYLEOS AARCH64: initial EL2\n' if el2 else
                  b'STYLEOS AARCH64: initial EL1\n') + b'STYLEOS AARCH64: current EL1\n'
        ram, source = (0x40000000, 0x48000000), (0x44000000, 0x44001000)
        reserved = [(0x9000000, 0x9001000), image, source]
        state.update(mode=0x30d00800, control0=4, control1=1, flags=0x3c0,
                     idt_base=base + 0x2000, idt_bytes=2048)
    prefix += (b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n' +
               f'STYLEOS HANDOFF PASS {arch}\n'.encode() + b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, spans in [('IMAGE', [image]), ('STACK', [stack]), ('HANDOFF', [handoff]),
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
    relocation = base if arch == 'x86_64' else 0
    raw = bytes.fromhex('90' if arch == 'x86_64' else '1f2003d5')
    site = dict(arch=arch, page_size=4096, image_start=image[0] - relocation,
                image_end=image[1] - relocation, image_size=image[1] - image[0],
                address_space='image-rva' if arch == 'x86_64' else 'identity-image-address',
                objects={name: dict(start=start-relocation, end=start+size-relocation,
                                    bytes=size, symbol='fixture_' + name)
                         for name, (start, size) in objects.items()},
                code_sites={})
    if arch == 'x86_64':
        site['preferred_image_base'] = 0x140000000
    else:
        site.update(bss_start=base + 0x4000, bss_end=image[1])
    for n, name in enumerate(_TASKS_CODE):
        address = base + 0x1000 + n * 16
        site['code_sites'][name] = dict(symbol=name, start=address-relocation,
            end=address-relocation+len(raw), bytes=len(raw), raw_offset=0,
            raw_hex=raw.hex(), instruction_hex=raw.hex(), sha256=hashlib.sha256(raw).hexdigest(),
            instructions=[dict(offset=0, bytes=raw.hex(), instruction='nop')],
            **{'rva' if arch == 'x86_64' else 'pc': address-relocation})
    site['integer_only'] = dict(compiler_flags=['-mgeneral-regs-only'],
        decoder_version='LLVM version 19.1.7', decoder_commands=[['llvm-objdump']],
        executable_spans=[dict(start=base, end=base+4, offset=0, section='.text', bytes=4,
                               sha256=hashlib.sha256(raw).hexdigest())],
        executable_sha256=hashlib.sha256(raw).hexdigest(), decoded_bytes=4, instruction_count=1)
    pc = lambda name: site['code_sites'][name]['start'] + relocation
    out = prefix
    for ident in ('A', 'B'):
        start, size = objects['stack_' + ident.lower()]
        out += bounds('TASKS STACK id=' + ident, start, start+size)
    for label, name in [('CONTROL', 'control'), ('MAP_WORKSPACE', 'diagnostic'),
                        ('DISPATCH_STACK', 'stack')]:
        start, size = objects[name]
        out += bounds('TASKS ' + label, start, start+size)
    out += line('TASKS STATE', **state)
    for ident in ('A', 'B'):
        start, size = objects['stack_' + ident.lower()]
        out += line('TASKS CREATE id=' + ident + ' state=READY',
                    sp=start+size-(56 if arch == 'x86_64' else 16),
                    pc=pc('styleos_tasks_bootstrap'), entry=pc('styleos_tasks_' + ident.lower()))
    for count, (ident, kind) in enumerate([('A', 'START'), ('B', 'START'), ('A', 'RESUME'),
                                          ('B', 'RESUME'), ('A', 'EXIT'), ('B', 'EXIT')], 1):
        start, _size = objects['stack_' + ident.lower()]
        seq, mask = (count+1)//2, 255 if arch == 'x86_64' else 2047
        tag = (0x100 if ident == 'A' else 0x200) + seq
        out += line('TASKS GO id=' + ident, count=count)
        out += line('TASKS EVENT id=' + ident + ' kind=' + kind, local=start+0x3e00,
                    value=0xa11ce55a12345678 if ident == 'A' else 0xb0b55aa987654321,
                    regs=0 if kind == 'START' else mask)
        if kind == 'EXIT':
            out += f'STYLEOS TASKS RETURN id={ident}\n'.encode()
        else:
            out += line('TASKS YIELD id=' + ident, seq=seq, tag=tag)
        out += line('TASKS SAVED id=' + ident + (' state=DONE' if kind == 'EXIT' else ' state=READY'),
                    sp=start+0x3d00, pc=pc('styleos_tasks_exit_resume' if kind == 'EXIT'
                                          else 'styleos_tasks_probe_resume'), seq=min(seq, 2),
                    mask=0 if kind == 'EXIT' else mask)
        if kind != 'EXIT':
            for index in range(8 if arch == 'x86_64' else 11):
                out += line('TASKS REG id=' + ident, seq=seq, index=index,
                            value=0x1122000000000000 + (index << 32) + tag)
        out += line('TASKS DISPATCH', local=stack[0]+0x800, value=0xd15ca11c12345678,
                    sp=stack[0]+0x700, pc=pc('styleos_tasks_dispatch_resume'))
    out += line('TASKS EMPTY', completed=2, dispatches=6)
    out += b'STYLEOS TASKS RELEASE id=A state=FREE\nSTYLEOS TASKS RELEASE id=B state=FREE\n'
    for ident in ('A', 'B'):
        out += line('TASKS CANARY id=' + ident, low0=0x6b6b6b6b6b6b6b6b,
                    low1=0x6b6b6b6b6b6b6b6b, high0=0xa5a5a5a5a5a5a5a5, high1=0xa5a5a5a5a5a5a5a5)
    out += b'STYLEOS TASKS CANARY PASS\n' + line('TASKS STATE AFTER', **state)
    return out + b'STYLEOS TASKS PASS\nSTYLEOS KERNEL HALT\n', site


class TaskTraceTests(unittest.TestCase):
    def test_both_isa_and_arm_initial_el_have_exact_six_dispatches(self):
        for arch, el2 in [('x86_64', False), ('aarch64', False), ('aarch64', True)]:
            with self.subTest(arch=arch, el2=el2):
                output, site = fixture(arch, el2)
                self.assertTrue(serial_tasks_ok(output, arch, site, el2=el2))
                report = tasks_report(output, arch, site, el2=el2)
                self.assertEqual([(row['id'], row['kind']) for row in report['schedule']],
                    [('A', 'START'), ('B', 'START'), ('A', 'RESUME'), ('B', 'RESUME'),
                     ('A', 'EXIT'), ('B', 'EXIT')])
                self.assertEqual(report['completed'], 2)
                self.assertEqual(report['released'], ['A', 'B'])
                self.assertEqual(len(report['canaries']), 2)

    def assert_rejected(self, output, site, arch='aarch64'):
        self.assertFalse(serial_tasks_ok(output, arch, site))
        self.assertIsNone(tasks_report(output, arch, site))

    def test_pass_only_timeout_missing_handoff_and_missing_halt_fail(self):
        output, site = fixture()
        for bad in (b'STYLEOS TASKS PASS\n', output+b'timed out\n', output+b'timeout\n',
                    output.replace(b'STYLEOS MEMMAP PASS\n', b''),
                    output.replace(b'STYLEOS KERNEL HALT\n', b''),
                    output.replace(b'STYLEOS HANDOFF PASS aarch64\n', b'')):
            self.assert_rejected(bad, site)

    def test_missing_duplicate_resume_exit_restart_and_wrong_order_fail(self):
        output, site = fixture()
        for kind in (b'EVENT id=A kind=RESUME', b'EVENT id=B kind=EXIT', b'RETURN id=A',
                     b'YIELD id=A', b'RELEASE id=A', b'EMPTY completed='):
            row = next(row for row in output.splitlines(keepends=True) if kind in row)
            self.assert_rejected(output.replace(row, b'', 1), site)
            self.assert_rejected(output.replace(row, row*2, 1), site)
        self.assert_rejected(output.replace(b'kind=EXIT', b'kind=START'), site)
        self.assert_rejected(output.replace(b'EVENT id=A kind=RESUME', b'EVENT id=B kind=RESUME'), site)
        self.assert_rejected(output.replace(b'STYLEOS TASKS PASS\n',
            b'STYLEOS TASKS GO id=A count=0x0000000000000007\nSTYLEOS TASKS PASS\n'), site)

    def test_local_sp_and_initial_frames_must_use_their_own_guarded_stacks(self):
        for arch in ('x86_64', 'aarch64'):
            output, site = fixture(arch)
            for key in (b'CREATE id=A', b'SAVED id=A', b'EVENT id=A', b'DISPATCH local='):
                row = next(row for row in output.splitlines(keepends=True) if key in row)
                field = b'local=' if b'EVENT' in row or b'DISPATCH' in row else b'sp='
                value = row.split(field)[1].split()[0]
                for replacement in (0, site['objects']['stack_b']['start'] + (0x60000000 if arch == 'x86_64' else 0),
                                    site['objects']['stack']['start'] + (0x60000000 if arch == 'x86_64' else 0),
                                    int(value, 16)+1):
                    bad_row = row.replace(field+value, field+f'0x{replacement:016x}'.encode())
                    self.assert_rejected(output.replace(row, bad_row, 1), site, arch)

    def test_pc_entries_context_masks_register_values_and_counters_are_bound(self):
        output, site = fixture()
        mutations = [(b'pc=0x0000000040081000', b'pc=0x0000000040081004'),
                     (b'entry=0x0000000040081010', b'entry=0x0000000040081020'),
                     (b'regs=0x00000000000007ff', b'regs=0x00000000000007fe'),
                     (b'mask=0x00000000000007ff', b'mask=0x0000000000000000'),
                     (b'value=0x1122000000000101', b'value=0x1122000000000102'),
                     (b'count=0x0000000000000003', b'count=0x0000000000000004'),
                     (b'tag=0x0000000000000102', b'tag=0x0000000000000101'),
                     (b'completed=0x0000000000000002', b'completed=0x0000000000000001'),
                     (b'dispatches=0x0000000000000006', b'dispatches=0x0000000000000007')]
        for old, new in mutations:
            with self.subTest(old=old):
                self.assertIn(old, output)
                self.assert_rejected(output.replace(old, new, 1), site)
        reg = next(row for row in output.splitlines(keepends=True) if b'REG id=A' in row)
        self.assert_rejected(output.replace(reg, b'', 1), site)

    def test_local_values_canaries_and_critical_hardware_state_are_preserved(self):
        output, site = fixture()
        for old, new in [(b'value=0xa11ce55a12345678', b'value=0xb0b55aa987654321'),
                         (b'value=0xd15ca11c12345678', b'value=0x0000000000000000'),
                         (b'high1=0xa5a5a5a5a5a5a5a5', b'high1=0xa5a5a5a5a5a5a5a4'),
                         (b'control0=0x0000000000000004', b'control0=0x0000000000000008'),
                         (b'control1=0x0000000000000001', b'control1=0x0000000000000000'),
                         (b'flags=0x00000000000003c0', b'flags=0x0000000000000000')]:
            self.assert_rejected(output.replace(old, new, 1), site)
        row = next(row for row in output.splitlines(keepends=True) if b'STATE AFTER' in row)
        self.assert_rejected(output.replace(row, row.replace(b'root=0x0000000000000000',
                                                            b'root=0x0000000000001000')), site)
        output, site = fixture('x86_64')
        row = next(row for row in output.splitlines(keepends=True) if b'STATE AFTER' in row)
        changed = output.replace(row, row.replace(b'flags=0x0000000000000002', b'flags=0x0000000000000046'))
        self.assertTrue(serial_tasks_ok(changed, 'x86_64', site))
        self.assert_rejected(output.replace(row, row.replace(b'flags=0x0000000000000002',
                                                            b'flags=0x0000000000000202')), site, 'x86_64')

    def test_complete_binary_ranges_control_source_and_code_bytes_are_required(self):
        output, site = fixture()
        for name, key, value in [('stack_a', 'bytes', 16383), ('stack_b', 'end', 0x40098001),
                                 ('control', 'start', site['objects']['stack_a']['start']),
                                 ('diagnostic', 'bytes', 12401)]:
            changed = deepcopy(site)
            changed['objects'][name][key] = value
            self.assert_rejected(output, changed)
        for name in ('stack_a', 'control'):
            changed = deepcopy(site)
            changed['objects'][name]['end'] -= 1
            self.assert_rejected(output, changed)
        for key, value in [('image_size', 0x4ffff), ('address_space', 'physical-pointer')]:
            self.assert_rejected(output, dict(site, **{key: value}))
        changed = deepcopy(site)
        changed['code_sites']['styleos_tasks_probe_resume']['sha256'] = '0'*64
        self.assert_rejected(output, changed)
        changed = deepcopy(site)
        del changed['integer_only']
        self.assert_rejected(output, changed)
        # Alter SOURCE and its reserved cover coherently: map remains valid,
        # but the new source overlaps the full task stack and must be refused.
        bad = output.replace(bounds('MEMMAP SOURCE', 0x44000000, 0x44001000),
                             bounds('MEMMAP SOURCE', 0x40090000, 0x40094000))
        self.assert_rejected(bad, site)

    def test_old_modes_reject_any_tasks_records(self):
        output, _site = fixture()
        prefix = output.split(b'STYLEOS TASKS STACK', 1)[0]
        self.assertTrue(serial_memory_ok(prefix+b'STYLEOS KERNEL HALT\n', 'aarch64', 'normal'))
        for marker in (b'STYLEOS TASKS PASS\n', b'STYLEOS TASKS ERROR: stack\n'):
            self.assertFalse(serial_memory_ok(prefix+marker+b'STYLEOS KERNEL HALT\n', 'aarch64', 'normal'))


def binding_fixture(folder, arch='aarch64'):
    target = folder / arch
    target.mkdir()
    flags = ['-mgeneral-regs-only'] if arch == 'aarch64' else [
        '-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
    flags += ['--target=' + ('aarch64-none-elf' if arch == 'aarch64' else 'x86_64-pc-win32-coff'),
              '-DSTYLEOS_TEST_TASKS=1']
    site = dict(arch=arch, integer_only=dict(compiler_flags=flags))
    manifest = dict(invalid_boot_info=False, invalid_memory_map=False,
                    test_exception=False, test_bootpool=False,
                    test_paging=False, test_paging_fault=False,
                    test_ownership_plan=False, test_ownership_plan_invalid=False,
                    test_tasks=True, compiler_flags={arch: flags}, task_sources={arch: _tasks_sources(arch)}, artifacts=[])
    names = [('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')] if arch == 'aarch64' else [
        ('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')]
    for name, key in names:
        data = ('fixture:' + name).encode()
        (target / name).write_bytes(data)
        site[key] = hashlib.sha256(data).hexdigest()
        manifest['artifacts'].append(dict(path=arch+'/'+name, size=len(data), sha256=site[key]))
    derived = site.copy()
    if arch == 'x86_64':
        del derived['disk_sha256']
        derived['map_sha256'] = site['map_sha256'] = 'f'*64
        (target / 'kernel.map').write_text('fixture linker map')
    (target / 'tasks-site.json').write_text(json.dumps(site))
    (folder / 'manifest.json').write_text(json.dumps(manifest))
    return site, derived, manifest


class TaskBindingTests(unittest.TestCase):
    def test_manifest_modes_are_exact_bools_and_sidecar_is_independently_rederived(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-binding-') as temporary:
            folder = Path(temporary)
            site, derived, baseline = binding_fixture(folder)
            with patch('tools.test.describe_tasks', return_value=derived) as describe:
                self.assertEqual(verified_tasks_site(folder, 'aarch64'), site)
                target = folder / 'aarch64'
                describe.assert_called_once_with('aarch64', target/'kernel.elf',
                    raw_path=target/'styleos.bin', compiler_flags=baseline['compiler_flags']['aarch64'])
                for key in ('invalid_boot_info', 'invalid_memory_map', 'test_exception',
                            'test_bootpool', 'test_paging', 'test_paging_fault',
                            'test_ownership_plan', 'test_ownership_plan_invalid', 'test_tasks'):
                    for value in (not baseline[key], int(baseline[key])):
                        changed = deepcopy(baseline)
                        changed[key] = value
                        (folder/'manifest.json').write_text(json.dumps(changed))
                        with self.subTest(key=key, value=value), self.assertRaisesRegex(ValueError, 'manifest'):
                            verified_tasks_site(folder, 'aarch64')
                (folder/'manifest.json').write_text(json.dumps(baseline))
                (target/'tasks-site.json').write_text(json.dumps(dict(site, binary_sha256='edited')))
                with self.assertRaisesRegex(ValueError, 'independently checked'):
                    verified_tasks_site(folder, 'aarch64')

    def test_missing_fake_or_conflicting_manifest_flags_cannot_replace_actual_flags(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-flags-') as temporary:
            folder = Path(temporary)
            _site, derived, baseline = binding_fixture(folder)
            for flags in (None, [], 'not-an-arch-map', {'aarch64': []},
                          {'aarch64': '-mgeneral-regs-only'}, {'aarch64': [True]},
                          {'aarch64': ['-mno-general-regs-only']}):
                changed = deepcopy(baseline)
                changed['compiler_flags'] = flags
                (folder/'manifest.json').write_text(json.dumps(changed))
                with patch('tools.test.describe_tasks', return_value=derived):
                    with self.subTest(flags=flags), self.assertRaisesRegex(ValueError, 'flags'):
                        verified_tasks_site(folder, 'aarch64')

    def test_manifest_sources_require_complete_profile_without_foreign_or_duplicate_units(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-sources-') as temporary:
            folder = Path(temporary)
            _site, derived, baseline = binding_fixture(folder)
            sources = baseline['task_sources']['aarch64']
            for replacement in (None, [], sources[:-1], sources+[sources[0]],
                                sources+['kernel/ownership.c'],
                                ['kernel/boot_pool_selftest.c', *sources[1:]]):
                changed = deepcopy(baseline)
                changed['task_sources'] = {'aarch64': replacement}
                (folder/'manifest.json').write_text(json.dumps(changed))
                with patch('tools.test.describe_tasks', return_value=derived):
                    with self.subTest(sources=replacement), self.assertRaisesRegex(ValueError, 'source'):
                        verified_tasks_site(folder, 'aarch64')

    def test_both_booted_artifacts_sizes_sha_and_pe_disk_are_bound(self):
        for arch in ('x86_64', 'aarch64'):
            with self.subTest(arch=arch), tempfile.TemporaryDirectory() as temporary:
                folder = Path(temporary)
                site, derived, baseline = binding_fixture(folder, arch)
                with patch('tools.test.describe_tasks', return_value=derived):
                    self.assertEqual(verified_tasks_site(folder, arch), site)
                    for key, value in [('size', True), ('size', 1), ('sha256', 'edited'),
                                       ('path', arch+'/other.bin')]:
                        changed = deepcopy(baseline)
                        changed['artifacts'][0][key] = value
                        (folder/'manifest.json').write_text(json.dumps(changed))
                        with self.assertRaisesRegex(ValueError, 'manifest'):
                            verified_tasks_site(folder, arch)
                    changed = deepcopy(baseline)
                    changed['artifacts'].append(changed['artifacts'][0].copy())
                    (folder/'manifest.json').write_text(json.dumps(changed))
                    with self.assertRaises(ValueError):
                        verified_tasks_site(folder, arch)
                    if arch == 'x86_64':
                        (folder/'manifest.json').write_text(json.dumps(baseline))
                        (folder/arch/'styleos.img').write_bytes(b'changed disk')
                        with self.assertRaisesRegex(ValueError, 'independently checked'):
                            verified_tasks_site(folder, arch)


class TaskInventoryTests(unittest.TestCase):
    def inspect_inventory(self, arches):
        temporary = tempfile.TemporaryDirectory(prefix='styleos-tasks-inventory-')
        self.addCleanup(temporary.cleanup)
        runner = Checks(Path(temporary.name))
        jobs = []
        def inspect(job):
            jobs.append(job)
            return job[0], 'PASS', 'inventory fixture', ''
        with (patch.object(runner, 'build', return_value=True) as builds,
              patch.object(runner, 'run_qemu', side_effect=inspect),
              patch.object(runner, 'record'), patch('tools.test.shutil.which', return_value='/fake/qemu'),
              patch('tools.test.firmware_path', return_value=Path('/fake/firmware')) as firmware):
            runner.qemu(25, arches)
        return runner, jobs, builds, firmware

    def test_full_inventory_keeps_old_27_qemu_jobs_and_adds_three_tasks_jobs(self):
        runner, jobs, builds, _firmware = self.inspect_inventory(('x86_64', 'aarch64'))
        added = [job for job in jobs if job[5] == 'tasks']
        self.assertEqual(len(jobs), 30)
        self.assertEqual(len(jobs)-len(added), 27)
        self.assertEqual({(j[1], j[6], j[7]) for j in added}, {
            ('x86_64', False, 128), ('aarch64', False, 128), ('aarch64', True, 128)})
        self.assertEqual(builds.call_count, 10)
        self.assertTrue(all(job[3] == runner.output/'tasks' for job in added))
        self.assertEqual(builds.call_args_list[-1].args,
                         (runner.output/'tasks', 'tasks', 'build-tasks', 'all'))

    def test_arm_only_inventory_has_two_tasks_jobs_and_no_x86_or_firmware(self):
        _runner, jobs, builds, firmware = self.inspect_inventory(('aarch64',))
        firmware.assert_not_called()
        self.assertEqual(len(jobs), 20)
        self.assertEqual(len([job for job in jobs if job[5] == 'tasks']), 2)
        self.assertTrue(all(call.args[3] == 'aarch64' for call in builds.call_args_list))
        self.assertTrue(all(job[1] == 'aarch64' and job[4] is None for job in jobs))

    def test_host_tasks_adds_exactly_one_c_stage_to_old_host_inventory(self):
        with tempfile.TemporaryDirectory() as temporary:
            runner = Checks(Path(temporary))
            with patch.object(runner, 'command', return_value=True) as commands:
                runner.host()
            self.assertEqual(commands.call_count, 9)
            task = [call for call in commands.call_args_list if call.args[0] == 'host-tasks']
            self.assertEqual(len(task), 1)
            self.assertIn('tests/test_tasks.c', task[0].args[1][0])
            self.assertIn('kernel/tasks.c', task[0].args[1][0])


@unittest.skipUnless(os.name == 'posix', 'POSIX pipe/signal lifecycle')
class TaskProcessTests(unittest.TestCase):
    def run_fake(self, folder, output, site, after='while True:\n    time.sleep(1)',
                 timeout=1.5, arch='aarch64', idt=True):
        fake = folder/'fake-qemu'
        fake.write_text(f'#!{sys.executable}\nimport os,signal,time\nsignal.alarm(5)\n' +
                        f'os.write(1, {output!r})\n' + after + '\n')
        fake.chmod(0o700)
        (folder/'manifest.json').write_text('{}')
        runner = Checks(folder)
        firmware = None
        if arch == 'x86_64':
            code, variables = folder/'code.fd', folder/'vars.fd'
            code.write_bytes(b'fixture code')
            variables.write_bytes(b'fixture vars')
            firmware = code, variables
        job = ('fixture-tasks', arch, str(fake), folder, firmware, 'tasks', False, 128, timeout)
        with (patch('tools.test.verified_tasks_site', return_value=site),
              patch('tools.test.serial_idt_ready', return_value=idt)):
            result = runner.run_qemu(job)
        summary = json.loads((runner.logs/'fixture-tasks.json').read_text())
        return result, summary

    def test_tasks_json_contains_bound_accepted_trace_and_intentional_halt_lifecycle(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-process-') as temporary:
            output, site = fixture()
            result, summary = self.run_fake(Path(temporary), output, site)
            self.assertEqual(result[1], 'PASS', result)
            self.assertEqual(summary['status'], 'PASS')
            self.assertEqual(summary['tasks_site'], site)
            self.assertEqual(len(summary['tasks_trace']['schedule']), 6)
            self.assertEqual(summary['uart_sha256'], hashlib.sha256(output).hexdigest())
            self.assertEqual(summary['manifest_sha256'], hashlib.sha256(b'{}').hexdigest())
            self.assertTrue(summary['requested_stop'])
            self.assertTrue(summary['settled'])
            self.assertFalse(summary['natural_exit'])

    def test_json_status_is_after_common_x86_idt_guard(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-idt-') as temporary:
            output, site = fixture('x86_64')
            result, summary = self.run_fake(Path(temporary), output, site, arch='x86_64', idt=False)
            self.assertEqual(result[1], 'FAIL')
            self.assertEqual(summary['status'], 'FAIL')
            self.assertIsNone(summary['tasks_trace'])

    def test_timeout_natural_exit_pass_only_and_reset_do_not_publish_accepted_trace(self):
        with tempfile.TemporaryDirectory(prefix='styleos-tasks-terminal-') as temporary:
            folder = Path(temporary)
            output, site = fixture()
            for data, after, timeout in [(b'STYLEOS TASKS PASS\n', 'while True:\n    time.sleep(1)', .1),
                                         (output, 'os._exit(0)', 1.5),
                                         (b'STYLEOS TASKS PASS\nSTYLEOS KERNEL HALT\n',
                                          'while True:\n    time.sleep(1)', 1.5)]:
                result, summary = self.run_fake(folder, data, site, after, timeout)
                self.assertEqual(result[1], 'FAIL')
                self.assertEqual(summary['status'], 'FAIL')
                self.assertIsNone(summary['tasks_trace'])
            self.assertTrue(self.run_fake(folder, output, site, 'os._exit(0)')[1]['natural_exit'])


if __name__ == '__main__':
    unittest.main()
