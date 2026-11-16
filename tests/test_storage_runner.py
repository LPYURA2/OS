"""Adversarial storage evidence/lifecycle tests; no QEMU or LLVM is required."""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import test_storage as runner

BASE = 0x40080000
OBJECT_ORDER = ('control', 'workspace', 'queue', 'request', 'vblk_control',
                'stack', 'diagnostic', 'pool', 'pool_metadata', 'vectors')
FLAGS = ['--target=aarch64-none-elf', '-DSTYLEOS_EXPECT_ARCH=2',
         '-DSTYLEOS_TEST_STORAGE=1', '-mgeneral-regs-only', '-mstrict-align']
MODES = ('invalid_boot_info', 'invalid_memory_map', 'test_exception',
         'test_bootpool', 'test_paging', 'test_paging_fault',
         'test_ownership_plan', 'test_ownership_plan_invalid',
         'test_tasks', 'test_user_processes')


def fields(label, **values):
    return ('STYLEOS STORAGE ' + label + ''.join(
        f' {key}=0x{value:016x}' for key, value in values.items()) + '\n').encode()


def bounds(label, start, end):
    return f'STYLEOS {label} 0x{start:016x} 0x{end:016x}\n'.encode()


def valid_site():
    """Independent finite extents, matching the public storage-site schema."""
    objects = dict(control=(BASE+0x22000, 4096), workspace=(BASE+0x23000, 4096),
        queue=(BASE+0x20000, 4096), request=(BASE+0x21000, 4096),
        vblk_control=(BASE+0x1f000, 4096), stack=(BASE+0xe080, 65536),
        diagnostic=(BASE+0xb000, 12400), pool=(BASE+0x2000, 32768),
        pool_metadata=(BASE+0xa000, 4096), vectors=(BASE+0x800, 2048))
    return dict(arch='aarch64', address_space='identity-image-address', page_size=4096,
        image_start=BASE, image_end=BASE+0x24000, image_size=0x24000,
        bss_start=BASE+0x2000, bss_end=BASE+0x24000,
        binary_sha256='a'*64, raw_sha256='b'*64, compiler_flags=FLAGS.copy(),
        image_entry_branch=0x14000010, image_entry_target=BASE+64,
        runtime_contract='fixed ARM Image; EL1h; MMU, data and instruction caches off',
        objects={key: dict(symbol='fixture_'+key, start=start, end=start+size,
                           bytes=size) for key, (start, size) in objects.items()})


def valid_trace(phase='init-a', site=None):
    """Build UART from the guest's public records, without its parser helpers."""
    site = valid_site() if site is None else site
    image = site['image_start'], site['image_end']
    stack = site['objects']['stack']['start'], site['objects']['stack']['end']
    handoff = stack[0]+0x100, stack[0]+0x160
    source = 0x44000000, 0x44001000
    prefix = (b'STYLEOS AARCH64: initial EL1\n'
              b'STYLEOS AARCH64: current EL1\n'
              b'STYLEOS AARCH64: DTB header OK\n'
              b'STYLEOS kernel_main entered\n'
              b'STYLEOS boot_info validated v1\n'
              b'STYLEOS HANDOFF PASS aarch64\n'
              b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    groups = [('IMAGE', [image]), ('STACK', [stack]), ('HANDOFF', [handoff]),
        ('SOURCE', [source]), ('RAM', [(0x40000000, 0x48000000)]),
        ('RESERVED', [(0x9000000, 0x9001000), image, source]),
        ('CANDIDATE', [(0x40000000, image[0]), (image[1], source[0]),
                       (source[1], 0x48000000)])]
    for kind, spans in groups:
        for start, end in spans:
            prefix += bounds('MEMMAP '+kind, start, end)
    prefix += b'STYLEOS MEMMAP PASS\n'
    out = prefix + f'STYLEOS STORAGE PHASE name={phase}\n'.encode()
    for name in OBJECT_ORDER:
        item = site['objects'][name]
        out += bounds('STORAGE OBJECT name='+name, item['start'], item['end'])
    out += fields('MACHINE', el=4, sctlr=0x30d00800, daif=0x3c0,
                  sp=stack[1]-0x100, vectors=site['objects']['vectors']['start'])
    out += fields('FDT', source=source[0], bytes=source[1]-source[0], candidates=32)
    out += fields('DEVICE', base=0xa003e00, bytes=0x200, version=2, id=2,
                  features=0x100000200, negotiated=0x100000200, sectors=2048, queue=8)

    def read(payload, generation):
        return (f'STYLEOS STORAGE READ generation=0x{generation:016x} '
            f'name={runner.FILE_NAME.hex()} length=0x{len(payload):016x} '
            f'data={payload.hex()}\n').encode()

    def progress(label, payload, generation, reads, writes, flushes):
        return fields(label, generation=generation, length=len(payload),
            payload_crc=zlib.crc32(payload.ljust(1024, b'\0')),
            reads=reads, writes=writes, flushes=flushes)

    def io(reads, writes, flushes):
        return fields('IO', reads=reads, writes=writes, flushes=flushes,
                      requests=reads+writes+flushes)

    if phase == 'init-a':
        out += b'STYLEOS STORAGE INIT EMPTY\n'
        out += progress('PREPARED', runner.PAYLOAD_A, 1, 2048, 5, 3)
        out += progress('COMMITTED', runner.PAYLOAD_A, 1, 2048, 6, 4)
        out += read(runner.PAYLOAD_A, 1) + io(2053, 6, 4)
    elif phase in ('update-b', 'prepare-b'):
        out += read(runner.PAYLOAD_A, 1)
        out += progress('PREPARED', runner.PAYLOAD_B, 2, 14, 4, 2)
        if phase == 'prepare-b':
            return out + io(14, 4, 2) + b'STYLEOS STORAGE BARRIER BEFORE_COMMIT\n'
        out += progress('COMMITTED', runner.PAYLOAD_B, 2, 14, 5, 3)
        out += read(runner.PAYLOAD_B, 2) + io(19, 5, 3)
    elif phase in ('read-a', 'recover-a', 'read-b'):
        out += read(runner.PAYLOAD_B if phase == 'read-b' else runner.PAYLOAD_A,
                    2 if phase == 'read-b' else 1) + io(14, 0, 0)
    else:
        raise ValueError('test fixture has no such phase')
    return out + b'STYLEOS STORAGE PASS\nSTYLEOS KERNEL HALT\n'


class StorageTraceTests(unittest.TestCase):
    def assert_rejected(self, output, phase='init-a', site=None):
        self.assertIsNone(runner.parse_storage_trace(
            output, phase, valid_site() if site is None else site))

    def test_all_six_phase_schemas_bind_exact_guest_observations(self):
        for phase in runner.PHASES:
            with self.subTest(phase=phase):
                trace = runner.parse_storage_trace(valid_trace(phase), phase, valid_site())
                self.assertIsInstance(trace, dict)
                self.assertTrue(trace)

    def test_pass_only_silence_partial_line_and_unknown_phase_are_refused(self):
        for output in (b'', b'STYLEOS STORAGE PASS\n',
                       b'STYLEOS STORAGE PASS\nSTYLEOS KERNEL HALT\n',
                       valid_trace()[:-1], valid_trace()+b'incomplete'):
            with self.subTest(output=output[-60:]): self.assert_rejected(output)
        self.assert_rejected(valid_trace(), phase='not-a-phase')

    def test_every_guest_record_is_mandatory_unique_and_ordered(self):
        output = valid_trace()
        lines = output.splitlines(keepends=True)
        for index, line in enumerate(lines):
            with self.subTest(index=index, mutation='missing'):
                self.assert_rejected(b''.join(lines[:index]+lines[index+1:]))
            with self.subTest(index=index, mutation='duplicate'):
                self.assert_rejected(b''.join(lines[:index]+[line]+lines[index:]))
        for index in range(len(lines)-1):
            changed = lines.copy()
            changed[index], changed[index+1] = changed[index+1], changed[index]
            with self.subTest(index=index, mutation='order'):
                self.assert_rejected(b''.join(changed))

    def test_success_cannot_erase_guest_error_exception_or_foreign_profile(self):
        for bad in (b'STYLEOS STORAGE ERROR stage=transport reason=timeout\n',
                    b'STYLEOS EXCEPTION arch=aarch64 kind=sync\n',
                    b'STYLEOS EMERGENCY HALT\n', b'STYLEOS BOOT_ERROR invalid\n',
                    b'STYLEOS USER PASS\n', b'STYLEOS OWNERSHIP PASS\n'):
            for position in (0, valid_trace().index(b'STYLEOS STORAGE PHASE'), len(valid_trace())):
                output = valid_trace()
                with self.subTest(bad=bad, position=position):
                    self.assert_rejected(output[:position]+bad+output[position:])

    def test_exact_file_name_length_bytes_and_generation_are_required(self):
        output = valid_trace()
        replacements = [(b'name='+runner.FILE_NAME.hex().encode(), b'name=6261642e747874'),
            (b'data='+runner.PAYLOAD_A.hex().encode(), b'data='+runner.PAYLOAD_B.hex().encode()),
            (b'length=0x000000000000000f data=', b'length=0x000000000000000e data='),
            (b'READ generation=0x0000000000000001', b'READ generation=0x0000000000000002')]
        for old, new in replacements:
            with self.subTest(old=old): self.assert_rejected(output.replace(old, new, 1))
        self.assert_rejected(valid_trace('read-a'), phase='read-b')
        self.assert_rejected(valid_trace('read-b'), phase='recover-a')

    def test_crc_flush_write_read_and_total_counters_are_exact(self):
        for phase in runner.PHASES:
            lines = valid_trace(phase).splitlines(keepends=True)
            for index, line in enumerate(lines):
                if line.startswith((b'STYLEOS STORAGE PREPARED ', b'STYLEOS STORAGE COMMITTED ',
                                    b'STYLEOS STORAGE IO ')):
                    for token in line.split():
                        if b'=0x' not in token: continue
                        key, value = token.split(b'=')
                        replacement = key + f'=0x{int(value,16)+1:016x}'.encode()
                        changed = lines.copy(); changed[index] = line.replace(token, replacement, 1)
                        with self.subTest(phase=phase, label=line.split()[2], key=key):
                            self.assert_rejected(b''.join(changed), phase)

    def test_before_commit_barrier_has_prepared_data_b_and_no_commit_or_pass(self):
        output = valid_trace('prepare-b')
        self.assert_rejected(output.replace(b'STYLEOS STORAGE BARRIER BEFORE_COMMIT\n',
            b'STYLEOS STORAGE PASS\nSTYLEOS KERNEL HALT\n'), 'prepare-b')
        self.assert_rejected(output+b'STYLEOS STORAGE COMMITTED generation=0x0000000000000002\n', 'prepare-b')
        self.assert_rejected(output+b'STYLEOS STORAGE PASS\n', 'prepare-b')
        self.assert_rejected(output.replace(b'STYLEOS STORAGE PREPARED ', b'STYLEOS STORAGE COMMITTED '), 'prepare-b')
        self.assert_rejected(output.replace(b'BARRIER BEFORE_COMMIT', b'BARRIER AFTER_COMMIT'), 'prepare-b')

    def test_device_needs_modern_version_flush_capacity_queue_and_fixed_mmio(self):
        output = valid_trace()
        for old, new in [(b'version=0x0000000000000002', b'version=0x0000000000000001'),
            (b'id=0x0000000000000002', b'id=0x0000000000000001'),
            (b'features=0x0000000100000200', b'features=0x0000000100000000'),
            (b'negotiated=0x0000000100000200', b'negotiated=0x0000000000000200'),
            (b'sectors=0x0000000000000800', b'sectors=0x0000000000000801'),
            (b'queue=0x0000000000000008', b'queue=0x0000000000000010'),
            (b'base=0x000000000a003e00', b'base=0x000000000a003e01'),
            (b'base=0x000000000a003e00', b'base=0x000000000b003e00'),
            (b'bytes=0x0000000000000200', b'bytes=0x0000000000000100')]:
            with self.subTest(old=old): self.assert_rejected(output.replace(old, new, 1))

    def test_machine_mode_stack_vector_and_fdt_are_bound_to_admitted_source(self):
        output = valid_trace()
        for old, new in [(b'el=0x0000000000000004', b'el=0x0000000000000008'),
            (b'sctlr=0x0000000030d00800', b'sctlr=0x0000000030d00801'),
            (b'sctlr=0x0000000030d00800', b'sctlr=0x0000000030d01800'),
            (b'daif=0x00000000000003c0', b'daif=0x0000000000000000'),
            (b'sp=0x000000004009df80', b'sp=0x000000004009df81'),
            (b'vectors=0x0000000040080800', b'vectors=0x0000000040081000'),
            (b'source=0x0000000044000000', b'source=0x0000000044001000'),
            (b'bytes=0x0000000000001000', b'bytes=0x0000000000000fff'),
            (b'candidates=0x0000000000000020', b'candidates=0x000000000000001f')]:
            self.assertIn(old, output)
            with self.subTest(old=old): self.assert_rejected(output.replace(old, new, 1))

    def test_every_complete_object_extent_is_bound_to_the_independent_site(self):
        output = valid_trace()
        for name in OBJECT_ORDER:
            item = valid_site()['objects'][name]
            old = bounds('STORAGE OBJECT name='+name, item['start'], item['end'])
            for start, end in ((item['start']+1, item['end']), (item['start'], item['end']-1)):
                changed = output.replace(old, bounds('STORAGE OBJECT name='+name, start, end))
                with self.subTest(name=name, start=start, end=end): self.assert_rejected(changed)
        for key, value in [('arch', 'x86_64'), ('image_start', BASE+4096), ('image_end', BASE+0x23000)]:
            site = valid_site(); site[key] = value
            with self.subTest(site_key=key): self.assert_rejected(output, site=site)

    def test_memmap_reservation_and_candidate_complement_are_verified(self):
        output = valid_trace()
        mutations = [(bounds('MEMMAP IMAGE', BASE, BASE+0x24000),
                      bounds('MEMMAP IMAGE', BASE, BASE+0x23000)),
            (bounds('MEMMAP HANDOFF', BASE+0xe180, BASE+0xe1e0),
             bounds('MEMMAP HANDOFF', BASE+0xe180, BASE+0xe1df)),
            (bounds('MEMMAP RESERVED', BASE, BASE+0x24000), b''),
            (bounds('MEMMAP CANDIDATE', 0x40000000, BASE),
             bounds('MEMMAP CANDIDATE', 0x40000000, BASE+4096)),
            (bounds('MEMMAP RAM', 0x40000000, 0x48000000),
             bounds('MEMMAP RAM', 0x40000000, 0x48001000))]
        for old, new in mutations:
            self.assertIn(old, output)
            with self.subTest(old=old): self.assert_rejected(output.replace(old, new, 1))


class StorageOutputOwnershipTests(unittest.TestCase):
    def test_repeated_invocation_preserves_old_disk_and_creates_new_exclusive_run(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)/'results'
            first = runner.new_run_directory(output)
            disk, state = runner.create_zero_disk(first, 'persist')
            second = runner.new_run_directory(output)
            new_disk, new_state = runner.create_zero_disk(second, 'persist')
            self.assertNotEqual(first, second)
            self.assertEqual(first.parent, second.parent)
            self.assertEqual(runner.disk_state(disk), state)
            self.assertEqual(disk.read_bytes(), bytes(runner.DISK_BYTES))
            self.assertEqual(new_disk.read_bytes(), bytes(runner.DISK_BYTES))
            self.assertNotEqual(state['inode'], new_state['inode'])
            with self.assertRaises(FileExistsError): runner.create_zero_disk(first, 'persist')
            with self.assertRaises(ValueError): runner.create_zero_disk(first, '../host-device')
            self.assertEqual(runner.disk_state(disk), state)

    def test_existing_evidence_and_symlink_destinations_are_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root/'evidence'; path.write_bytes(b'keep evidence')
            with self.assertRaises(FileExistsError): runner.write_new(path, b'replacement')
            link = root/'linked-output'; link.symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError): runner.new_run_directory(link)
            run = runner.new_run_directory(root/'output')
            (run/'disks').rmdir(); (run/'disks').symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError): runner.create_zero_disk(run, 'persist')
            self.assertEqual(path.read_bytes(), b'keep evidence')
            self.assertFalse((root/'persist.img').exists())

    def test_disk_identity_size_regular_type_hardlinks_and_symlinks_are_checked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run = runner.new_run_directory(root/'output')
            disk, state = runner.create_zero_disk(run, 'crash')
            altered = dict(state, inode=state['inode']+1)
            with self.assertRaises(ValueError): runner.disk_state(disk, altered)
            hardlink = root/'hardlink'; os.link(disk, hardlink)
            with self.assertRaises(ValueError): runner.disk_state(disk)
            hardlink.unlink()
            symbolic = root/'symbolic'; symbolic.symlink_to(disk)
            with self.assertRaises(OSError): runner.disk_state(symbolic)
            with disk.open('r+b') as stream: stream.truncate(runner.DISK_BYTES-1)
            with self.assertRaises(ValueError): runner.disk_state(disk)
            with self.assertRaises((ValueError, IsADirectoryError)): runner.disk_state(root)

    def test_qemu_uses_json_blockdev_for_untrusted_filename_punctuation(self):
        with tempfile.TemporaryDirectory(prefix='styleos,path,') as temporary:
            folder = Path(temporary)
            image = folder/'kernel,read-only=on.bin'; image.write_bytes(b'boot')
            disk = folder/'disk,read-only=on.img'; disk.touch()
            command = runner.qemu_command('/qemu', image, disk, 'prepare-b')
            nodes = [json.loads(command[i+1]) for i, arg in enumerate(command) if arg == '-blockdev']
            self.assertEqual(len(nodes), 2)
            self.assertEqual(nodes[0]['filename'], str(disk.resolve()))
            self.assertEqual(nodes[0]['driver'], 'file')
            self.assertFalse(nodes[0]['cache']['no-flush'])
            self.assertFalse(nodes[1]['read-only'])
            self.assertIn('virtio-mmio.force-legacy=false', command)
            self.assertEqual(command[command.index('-append')+1], 'styleos.storage=prepare-b')
            self.assertNotIn('-snapshot', command)
            with self.assertRaises(ValueError): runner.qemu_command('/qemu', image, disk, 'init')


def binding_fixture(folder):
    target = folder/'aarch64'; target.mkdir()
    site = valid_site()
    sources = runner.storage_sources()
    manifest = dict(test_storage=True, compiler_flags={'aarch64': FLAGS.copy()},
                    storage_sources={'aarch64': sources}, artifacts=[], **{key: False for key in MODES})
    for name, key, data in [('kernel.elf', 'binary_sha256', b'ELF fixture'),
                            ('styleos.bin', 'raw_sha256', b'raw Image fixture')]:
        (target/name).write_bytes(data)
        site[key] = hashlib.sha256(data).hexdigest()
        manifest['artifacts'].append(dict(path='aarch64/'+name, size=len(data), sha256=site[key]))
    (target/'storage-site.json').write_text(json.dumps(site))
    (target/'storage-compiler.json').write_text(json.dumps(dict(flags=FLAGS, sources=sources)))
    (folder/'manifest.json').write_text(json.dumps(manifest))
    return site, manifest


class StorageBindingTests(unittest.TestCase):
    def test_sidecar_is_independently_rederived_with_actual_elf_image_and_flags(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); site, manifest = binding_fixture(folder)
            with patch('tools.storage_site.describe_storage', return_value=site) as describe:
                self.assertEqual(runner.verified_storage_site(folder), site)
                describe.assert_called_once_with('aarch64', folder/'aarch64/kernel.elf',
                    raw_path=folder/'aarch64/styleos.bin', compiler_flags=FLAGS)
                changed = dict(site, image_end=site['image_end']+4096)
                (folder/'aarch64/storage-site.json').write_text(json.dumps(changed))
                with self.assertRaises(ValueError): runner.verified_storage_site(folder)

    def test_every_profile_mode_is_an_exact_boolean_and_other_profiles_are_absent(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); site, baseline = binding_fixture(folder)
            for key in ('test_storage', *MODES):
                for value in (not baseline[key], int(baseline[key]), None):
                    changed = deepcopy(baseline); changed[key] = value
                    (folder/'manifest.json').write_text(json.dumps(changed))
                    with patch('tools.storage_site.describe_storage', return_value=site):
                        with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                            runner.verified_storage_site(folder)

    def test_compiler_flags_sources_metadata_and_artifact_sha_sizes_are_bound(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); site, baseline = binding_fixture(folder)
            changes = []
            for flags in (None, [], FLAGS[:-1]+[True], FLAGS+['-DSTYLEOS_TEST_TASKS=1'],
                          FLAGS+['-DSTYLEOS_TEST_STORAGE=1'], FLAGS[1:],
                          FLAGS+['--target=x86_64-pc-win32-coff'],
                          FLAGS+['-DSTYLEOS_EXPECT_ARCH=1'], FLAGS+['-USTYLEOS_TEST_STORAGE']):
                item = deepcopy(baseline); item['compiler_flags'] = {'aarch64': flags}; changes.append(item)
            for key in ('compiler_flags', 'storage_sources'):
                item = deepcopy(baseline); item[key]['x86_64'] = []; changes.append(item)
            sources = baseline['storage_sources']['aarch64']
            for replacement in (None, [], sources[:-1], sources+[sources[0]], sources[::-1],
                                sources[:-1]+['kernel/tasks.c']):
                item = deepcopy(baseline); item['storage_sources'] = {'aarch64': replacement}; changes.append(item)
            for key, value in [('sha256', 'edited'), ('size', True), ('size', 0), ('path', 'other')]:
                item = deepcopy(baseline); item['artifacts'][0][key] = value; changes.append(item)
            item = deepcopy(baseline); item['artifacts'].append(item['artifacts'][0]); changes.append(item)
            for index, changed in enumerate(changes):
                (folder/'manifest.json').write_text(json.dumps(changed))
                with patch('tools.storage_site.describe_storage', return_value=site):
                    with self.subTest(index=index), self.assertRaises(ValueError): runner.verified_storage_site(folder)
            (folder/'manifest.json').write_text(json.dumps(baseline))
            (folder/'aarch64/storage-compiler.json').write_text(json.dumps(dict(flags=FLAGS, sources=sources[:-1])))
            with patch('tools.storage_site.describe_storage', return_value=site):
                with self.assertRaises(ValueError): runner.verified_storage_site(folder)


@unittest.skipUnless(os.name == 'posix', 'POSIX signal and pipe lifecycle')
class StorageProcessTests(unittest.TestCase):
    def run_fake(self, output, phase='init-a', after='while True:\n    time.sleep(1)', timeout=1.5):
        code = ('import os,signal,time\nsignal.alarm(5)\n' +
                f'os.write(1, {output!r})\n' + after + '\n')
        return runner.collect_vm([sys.executable, '-c', code], phase, valid_site(), timeout)

    def test_valid_halt_is_observed_before_intentional_sigterm_and_bound_result(self):
        output = valid_trace()
        result = self.run_fake(output)
        self.assertEqual(result['status'], 'PASS', result)
        self.assertTrue(result['requested_stop'])
        self.assertTrue(result['settled'])
        self.assertFalse(result['natural_exit'])
        self.assertFalse(result['forced_stop'])
        self.assertFalse(result['barrier_validated'])
        self.assertEqual(result['stop_signal'], signal.SIGTERM)
        self.assertEqual(result['returncode'], -signal.SIGTERM)
        self.assertEqual(result['uart_sha256'], hashlib.sha256(output).hexdigest())
        self.assertEqual(result['uart'], output)
        self.assertIsInstance(result['pid'], int)
        self.assertIsInstance(result['trace'], dict)

    def test_only_valid_prepare_barrier_uses_sigkill(self):
        result = self.run_fake(valid_trace('prepare-b'), 'prepare-b')
        self.assertEqual(result['status'], 'PASS', result)
        self.assertTrue(result['barrier_validated'])
        self.assertEqual(result['stop_signal'], signal.SIGKILL)
        self.assertEqual(result['returncode'], -signal.SIGKILL)
        self.assertFalse(result['natural_exit'])
        self.assertTrue(result['settled'])

    def test_timeout_error_pass_only_reset_and_unvalidated_barrier_fail(self):
        cases = [(b'', 'prepare-b', 'while True:\n    time.sleep(1)', .12),
            (b'STYLEOS STORAGE ERROR stage=transport reason=timeout\n', 'prepare-b',
             'while True:\n    time.sleep(1)', 1.5),
            (b'STYLEOS STORAGE BARRIER BEFORE_COMMIT\n', 'prepare-b',
             'while True:\n    time.sleep(1)', 1.5),
            (valid_trace('prepare-b'), 'prepare-b', 'os._exit(0)', 1.5),
            (valid_trace(), 'init-a', 'os._exit(0)', 1.5),
            (b'STYLEOS STORAGE PASS\nSTYLEOS KERNEL HALT\n', 'init-a',
             'while True:\n    time.sleep(1)', 1.5)]
        for output, phase, after, timeout in cases:
            with self.subTest(phase=phase, after=after, output=output[-60:]):
                result = self.run_fake(output, phase, after, timeout)
                self.assertEqual(result['status'], 'FAIL', result)
                self.assertFalse(result['barrier_validated'])
                self.assertIsNone(result['trace'])

    def test_error_after_barrier_and_late_output_after_stop_invalidate_success(self):
        result = self.run_fake(valid_trace('prepare-b'), 'prepare-b',
            'time.sleep(.05)\nos.write(1,b"STYLEOS STORAGE ERROR stage=store reason=corrupt\\n")\n'
            'while True:\n    time.sleep(1)')
        self.assertEqual(result['status'], 'FAIL', result)
        self.assertIsNone(result['trace'])
        setup = ('def stop(signum, frame):\n'
                 '    os.write(1,b"STYLEOS STORAGE ERROR stage=store reason=corrupt\\n")\n'
                 '    os._exit(0)\n'
                 'signal.signal(signal.SIGTERM, stop)\n'
                 'while True:\n    time.sleep(1)')
        result = self.run_fake(valid_trace(), after=setup)
        self.assertEqual(result['status'], 'FAIL', result)
        self.assertIsNone(result['trace'])


class StorageInventoryTests(unittest.TestCase):
    def test_seven_phases_include_four_persistence_and_three_crash_vms(self):
        self.assertEqual(runner.STAGES, (
            ('persist-init-a', 'persist', 'init-a'),
            ('persist-read-a', 'persist', 'read-a'),
            ('persist-update-b', 'persist', 'update-b'),
            ('persist-read-b', 'persist', 'read-b'),
            ('crash-init-a', 'crash', 'init-a'),
            ('crash-prepare-b', 'crash', 'prepare-b'),
            ('crash-recover-a', 'crash', 'recover-a')))

    def test_one_command_host_inventory_runs_three_portable_c_models_and_python(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = runner.new_run_directory(Path(temporary)/'output')
            checks = runner.StorageChecks(run)
            with patch.object(checks, 'command', return_value=True) as commands:
                self.assertTrue(checks.host())
            self.assertEqual([call.args[0] for call in commands.call_args_list],
                ['host-storage-format', 'host-storage-fdt', 'host-storage-transport', 'host-storage-python'])
            for call in commands.call_args_list[:3]:
                self.assertEqual(len(call.args[1]), 2)
                self.assertEqual(call.args[1][1], [run/'host-tests'/call.args[0]])
            transport = commands.call_args_list[2].args[1][0]
            self.assertIn('-DSTYLEOS_VBLK_HOST_TEST=1', transport)
            self.assertIn('arch/aarch64/virtio_block.c', transport)
            self.assertIn('test_storage*.py', commands.call_args_list[3].args[1][0])

    def test_execute_opens_two_runner_disks_and_records_seven_distinct_processes(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = runner.new_run_directory(Path(temporary)/'output')
            calls = []
            def vm(checks, name, disk, identity, phase, qemu, boot, site):
                state = runner.disk_state(disk, identity)
                calls.append((name, disk, phase))
                return dict(status='PASS', stage=name, phase=phase, pid=100+len(calls),
                            command=['fake-qemu', phase], disk_before=state, disk_after=state)
            with (patch.object(runner.StorageChecks, 'host', return_value=True),
                  patch.object(runner.StorageChecks, 'command', return_value=True),
                  patch.object(runner.StorageChecks, 'vm', autospec=True, side_effect=vm),
                  patch('tools.test_storage.verified_storage_site', return_value=valid_site()),
                  patch('tools.test_storage.metadata_snapshot', return_value={'fixture': 'a'*64}),
                  patch('tools.test_storage.source_snapshot', return_value={'fixture.py': 'b'*64}),
                  patch('tools.test_storage.subprocess.check_output', return_value='fixture'),
                  patch('tools.test_storage.shutil.which', return_value='/fake/qemu'),
                  patch('tools.test_storage.subprocess.run', return_value=
                        subprocess.CompletedProcess(['/fake/qemu', '--version'], 0, 'QEMU fixture')),
                  patch('builtins.print')):
                checks, provenance = runner.execute(run, 1.5)
            self.assertEqual([(name, phase) for name, _disk, phase in calls],
                             [(name, phase) for name, _disk, phase in runner.STAGES])
            self.assertEqual(len({disk for _name, disk, _phase in calls}), 2)
            self.assertEqual(len(provenance['qemu_processes']), 7)
            self.assertEqual(len({row['pid'] for row in provenance['qemu_processes']}), 7)
            self.assertEqual(set(provenance['initial_disks']), {'persist', 'crash'})
            self.assertTrue(all(row['status'] == 'PASS' for row in checks.results))

    def test_vm_checks_readonly_hash_and_records_sha_bound_disk_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = runner.new_run_directory(Path(temporary)/'output')
            boot = run/'boot'; (boot/'aarch64').mkdir(parents=True)
            (boot/'aarch64/styleos.bin').write_bytes(b'boot fixture')
            (boot/'manifest.json').write_bytes(b'{}')
            disk, state = runner.create_zero_disk(run, 'persist')
            checks = runner.StorageChecks(run); checks.timeout = 1.5
            for index, (phase, changed, expected) in enumerate([
                    ('read-a', False, 'PASS'), ('read-b', True, 'FAIL'),
                    ('init-a', False, 'FAIL'), ('update-b', True, 'PASS')]):
                output = valid_trace(phase)
                collected = dict(status='PASS', detail='fixture halt', pid=100+index,
                    command=['fake-qemu'], phase=phase, uart=output,
                    uart_sha256=hashlib.sha256(output).hexdigest(), requested_stop=True,
                    forced_stop=False, natural_exit=False, settled=True,
                    barrier_validated=False, stop_signal=signal.SIGTERM,
                    returncode=-signal.SIGTERM,
                    trace=runner.parse_storage_trace(output, phase, valid_site()))
                after = dict(state, sha256='c'*64) if changed else state.copy()
                name = 'vm-fixture-'+str(index)
                with (patch('tools.test_storage.collect_vm', return_value=collected),
                      patch('tools.test_storage.disk_state', side_effect=[state, after]),
                      patch('builtins.print')):
                    result = checks.vm(name, disk, state, phase, '/qemu', boot, valid_site())
                self.assertEqual(result['status'], expected)
                saved = json.loads((checks.logs/(name+'.json')).read_text())
                self.assertEqual(saved['status'], expected)
                self.assertEqual(saved['disk_before'], state)
                self.assertEqual(saved['disk_after'], after)
                self.assertEqual(saved['binary_sha256'], valid_site()['binary_sha256'])
                self.assertEqual(saved['raw_sha256'], valid_site()['raw_sha256'])
                self.assertEqual(saved['manifest_sha256'], hashlib.sha256(b'{}').hexdigest())
                self.assertEqual((checks.logs/(name+'.uart')).read_bytes(), output)


if __name__ == '__main__':
    unittest.main()
