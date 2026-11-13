"""Reject incomplete/adversarial boot-pool evidence; fixtures are not QEMU logs."""
from pathlib import Path
import hashlib
import json
import os
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.test import (Checks, serial_handoff_ok, serial_memory_ok,
                        serial_pool_ok, verified_pool_site)


def row(kind, start, end, prefix='MEMMAP'):
    return f'STYLEOS {prefix} {kind} 0x{start:016x} 0x{end:016x}\r\n'.encode()


def fixture(arch='aarch64', el2=False):
    if arch == 'x86_64':
        base = 0x60000000
        out = (b'STYLEOS x86_64 ExitBootServices OK\n'
               b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        source = base + 0x50000, base + 0x51000
        ram = [(base, base + 0x80000)]
        reserved = [(base, base + 0x40000), source]
        candidates = [(base + 0x40000, source[0]), (source[1], base + 0x80000)]
    else:
        base = 0x40080000
        out = (b'STYLEOS AARCH64: initial EL2\n' if el2 else b'STYLEOS AARCH64: initial EL1\n')
        out += b'STYLEOS AARCH64: current EL1\n'
        source = 0x44000000, 0x44001000
        ram = [(0x40000000, 0x48000000)]
        reserved = [(0x09000000, 0x09001000), (base, base + 0x40000), source]
        candidates = [(0x40000000, base), (base + 0x40000, source[0]),
                      (source[1], 0x48000000)]
    out += (b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n' +
            f'STYLEOS HANDOFF PASS {arch}\n'.encode() + b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, ranges in [('IMAGE', [(base, base + 0x40000)]),
                         ('STACK', [(base + 0x20000, base + 0x30000)]),
                         ('HANDOFF', [(base + 0x31000, base + 0x31060)]),
                         ('SOURCE', [source]), ('RAM', ram),
                         ('RESERVED', reserved), ('CANDIDATE', candidates)]:
        for start, end in ranges:
            out += row(kind, start, end)
    out += b'STYLEOS MEMMAP PASS\n'
    pool, metadata, workspace = ((base + 0x8000, base + 0x10000),
                                 (base + 0x10000, base + 0x11000),
                                 (base + 0x12000, base + 0x1a000))
    for name, bounds in [('RANGE', pool), ('META', metadata), ('WORKSPACE', workspace)]:
        out += row(name, *bounds, prefix='BOOTPOOL')
    out += b'STYLEOS BOOTPOOL ADMIT PASS\n'
    for index in range(8):
        out += f'STYLEOS BOOTPOOL ALLOC {index} 0x{pool[0] + index * 4096:016x} ZERO\n'.encode()
    out += (b'STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096\n'
            b'STYLEOS BOOTPOOL EXHAUSTED PASS\n' +
            f'STYLEOS BOOTPOOL REUSE 0x{pool[0] + 3 * 4096:016x} ZERO\n'.encode() +
            b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n')
    site = dict(arch=arch, pool_bytes=32768, metadata_bytes=4096, page_size=4096,
                image_size=0x40000)
    if arch == 'x86_64':
        site.update(address_space='image-rva', image_start=0, image_end=0x40000,
                    preferred_image_base=0x140000000, pool_start=0x8000,
                    pool_end=0x10000, metadata_start=0x10000, metadata_end=0x11000,
                    pool_rva_start=0x8000, pool_rva_end=0x10000,
                    metadata_rva_start=0x10000, metadata_rva_end=0x11000)
    else:
        site.update(address_space='identity-image-address', image_start=base,
                    image_end=base + 0x40000, bss_start=base + 0x7000,
                    bss_end=base + 0x40000, pool_start=pool[0], pool_end=pool[1],
                    metadata_start=metadata[0], metadata_end=metadata[1])
    return out, site


class PoolOutputTests(unittest.TestCase):
    def test_both_architectures_and_arm_initial_els(self):
        for arch, el2 in [('x86_64', False), ('aarch64', False), ('aarch64', True)]:
            with self.subTest(arch=arch, el2=el2):
                output, site = fixture(arch, el2)
                self.assertTrue(serial_pool_ok(output, arch, site, el2=el2))
                if arch == 'aarch64':
                    self.assertFalse(serial_pool_ok(output, arch, site, el2=not el2))

    def test_pe_pool_proof_uses_runtime_base_instead_of_preferred_base(self):
        output, site = fixture('x86_64')
        self.assertTrue(serial_pool_ok(output, 'x86_64', site))
        first = output.index(b'STYLEOS BOOTPOOL RANGE')
        # Preserve the valid live map, but make pool records pretend that
        # firmware used the linker's preferred base instead of its actual one.
        wrong = output[:first] + re.sub(
            rb'0x([0-9a-f]{16})',
            lambda match: f'0x{int(match[1], 16) - 0x60000000 + site["preferred_image_base"]:016x}'.encode(),
            output[first:])
        self.assertFalse(serial_pool_ok(wrong, 'x86_64', site))
        self.assertFalse(serial_pool_ok(output, 'x86_64', dict(site, pool_rva_end=0x11000)))

    def test_empty_pass_only_and_missing_real_prefix_do_not_pass(self):
        output, site = fixture()
        first = output.index(b'STYLEOS BOOTPOOL RANGE')
        for bad in (b'', b'STYLEOS BOOTPOOL PASS\n', output[first:],
                    b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n',
                    output[:first] + b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n'):
            with self.subTest(bad=bad[:70]):
                self.assertFalse(serial_pool_ok(bad, 'aarch64', site))

    def test_exact_order_and_terminal_halt_are_required(self):
        output, site = fixture()
        admit = b'STYLEOS BOOTPOOL ADMIT PASS\n'
        rw = b'STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096\n'
        exhausted = b'STYLEOS BOOTPOOL EXHAUSTED PASS\n'
        passed = b'STYLEOS BOOTPOOL PASS\n'
        halt = b'STYLEOS KERNEL HALT\n'
        for bad in (output.replace(admit, b''), output.replace(rw + exhausted, exhausted + rw),
                    output.replace(passed + halt, halt + passed), output.replace(halt, b''),
                    output + passed, output + halt,
                    output.replace(b'STYLEOS MEMMAP PASS\n', passed),
                    output.replace(exhausted, exhausted + b'STYLEOS EXCEPTION unexpected\n')):
            self.assertFalse(serial_pool_ok(bad, 'aarch64', site))

    def test_all_eight_indices_unique_aligned_addresses_and_reuse_must_match(self):
        output, site = fixture()
        for old, new in [
            (b'ALLOC 7', b'ALLOC 6'),
            (b'ALLOC 7 0x000000004008f000', b'ALLOC 7 0x000000004008e000'),
            (b'ALLOC 7 0x000000004008f000', b'ALLOC 7 0x0000000040090000'),
            (b'ALLOC 7 0x000000004008f000', b'ALLOC 7 0x000000004008f001'),
            (b'ALLOC 0 0x0000000040088000 ZERO', b'ALLOC 0 0x40088000 ZERO'),
            (b'ALLOC 0 0x0000000040088000 ZERO', b'ALLOC 0 0x0000000040088000'),
            (b'REUSE 0x000000004008b000', b'REUSE 0x000000004008c000'),
            (b'pages=8 bytes=4096', b'pages=8 bytes=1'),
        ]:
            with self.subTest(change=(old, new)):
                self.assertIn(old, output)
                self.assertFalse(serial_pool_ok(output.replace(old, new), 'aarch64', site))

    def test_full_bounds_metadata_and_workspace_are_checked(self):
        output, site = fixture()
        for old, new in [
            (row('RANGE', 0x40088000, 0x40090000, 'BOOTPOOL'),
             row('RANGE', 0x40088000, 0x40091000, 'BOOTPOOL')),
            (row('META', 0x40090000, 0x40091000, 'BOOTPOOL'),
             row('META', 0x40090000, 0x40091001, 'BOOTPOOL')),
            (row('META', 0x40090000, 0x40091000, 'BOOTPOOL'),
             row('META', 0x4008f000, 0x40090000, 'BOOTPOOL')),
            (row('WORKSPACE', 0x40092000, 0x4009a000, 'BOOTPOOL'),
             row('WORKSPACE', 0x4008f000, 0x4009a000, 'BOOTPOOL')),
            (row('WORKSPACE', 0x40092000, 0x4009a000, 'BOOTPOOL'),
             row('WORKSPACE', 0x400c0000, 0x400c1000, 'BOOTPOOL')),
        ]:
            with self.subTest(change=(old, new)):
                self.assertIn(old, output)
                self.assertFalse(serial_pool_ok(output.replace(old, new), 'aarch64', site))

    def test_stack_handoff_and_source_are_exclusions_for_the_whole_span(self):
        output, site = fixture()
        for kind, previous, replacement in [
            ('STACK', (0x400a0000, 0x400b0000), (0x4008f000, 0x4009f000)),
            ('HANDOFF', (0x400b1000, 0x400b1060), (0x4008fff0, 0x40090050)),
            ('SOURCE', (0x44000000, 0x44001000), (0x40090ff0, 0x40091000)),
        ]:
            with self.subTest(kind=kind):
                bad = output.replace(row(kind, *previous), row(kind, *replacement))
                self.assertFalse(serial_pool_ok(bad, 'aarch64', site))

    def test_memory_reservations_and_candidates_remain_independently_checked(self):
        output, site = fixture()
        for bad in (output.replace(row('RESERVED', 0x40080000, 0x400c0000), b''),
                    output.replace(row('CANDIDATE', 0x40000000, 0x40080000),
                                   row('CANDIDATE', 0x40000000, 0x40081000)),
                    output.replace(b'STYLEOS MEMMAP PASS\n', b'')):
            self.assertFalse(serial_pool_ok(bad, 'aarch64', site))

    def test_site_types_sizes_address_spaces_and_complete_bss_are_validated(self):
        output, site = fixture()
        for changes in [dict(arch='x86_64'), dict(address_space='physical'),
                        dict(pool_start=True), dict(pool_end='0x40090000'),
                        dict(pool_end=1 << 64), dict(pool_bytes=4096),
                        dict(metadata_bytes=1), dict(page_size=8192),
                        dict(bss_end=site['pool_end'] - 1),
                        dict(image_end=site['image_end'] - 4096),
                        dict(image_size=0x10000)]:
            with self.subTest(changes=changes):
                self.assertFalse(serial_pool_ok(output, 'aarch64', dict(site, **changes)))
        self.assertFalse(serial_pool_ok(output, 'aarch64', None))

    def test_error_and_timeout_cannot_be_hidden_by_success_markers(self):
        output, site = fixture()
        for bad in (b'timeout\n' + output, output + b'QEMU timed out\n',
                    output + b'STYLEOS BOOTPOOL ERROR: zero check\n',
                    output + b'STYLEOS BOOT_INFO ERROR: magic\n',
                    output + b'STYLEOS EMERGENCY HALT\n'):
            self.assertFalse(serial_pool_ok(bad, 'aarch64', site))

    def test_old_modes_reject_any_unexpected_pool_execution(self):
        output, _site = fixture()
        prefix = output[:output.index(b'STYLEOS BOOTPOOL RANGE')]
        normal = prefix + b'STYLEOS KERNEL HALT\n'
        self.assertTrue(serial_memory_ok(normal, 'aarch64', 'normal'))
        for marker in (b'STYLEOS BOOTPOOL RANGE invalid\n',
                       b'STYLEOS BOOTPOOL PASS\n', b'STYLEOS BOOTPOOL ERROR: test\n'):
            self.assertFalse(serial_handoff_ok(prefix + marker, 'aarch64'))
            self.assertFalse(serial_memory_ok(normal + marker, 'aarch64', 'normal'))
            self.assertFalse(serial_memory_ok(b'STYLEOS BOOT_INFO ERROR: magic\n' + marker,
                                              'aarch64', 'invalid'))
            self.assertFalse(serial_memory_ok(prefix.split(b'STYLEOS MEMMAP')[0] +
                                              b'STYLEOS MEMMAP ERROR: fdt-format\n' + marker,
                                              'aarch64', 'invalid-map'))


class PoolProofBindingTests(unittest.TestCase):
    def test_static_proof_is_rederived_and_edited_sidecar_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='styleos-pool-proof-') as temporary:
            folder = Path(temporary)
            target = folder / 'aarch64'
            target.mkdir()
            site = {'arch': 'aarch64', 'pool_start': 0x40088000}
            sidecar = target / 'pool-site.json'
            sidecar.write_text(json.dumps(site))
            # The pool-site helper's ELF/raw validation has its own binary
            # fixture tests; here the runner must call it rather than trust JSON.
            with patch('tools.test.describe_pool', return_value=site.copy()) as derive:
                self.assertEqual(verified_pool_site(folder, 'aarch64'), site)
                derive.assert_called_once_with('aarch64', target / 'kernel.elf',
                                               raw_path=target / 'styleos.bin')
            sidecar.write_text(json.dumps(dict(site, pool_start=0x40089000)))
            with patch('tools.test.describe_pool', return_value=site):
                with self.assertRaisesRegex(ValueError, 'independently checked'):
                    verified_pool_site(folder, 'aarch64')

    def test_actual_pe_disk_hash_is_required_alongside_binary_map_proof(self):
        with tempfile.TemporaryDirectory(prefix='styleos-pool-disk-') as temporary:
            folder = Path(temporary)
            target = folder / 'x86_64'
            target.mkdir()
            disk = target / 'styleos.img'
            disk.write_bytes(b'test disk bytes, not a boot image')
            derived = {'arch': 'x86_64', 'binary_sha256': 'binary', 'map_sha256': 'map'}
            site = dict(derived, disk_sha256=hashlib.sha256(disk.read_bytes()).hexdigest())
            (target / 'pool-site.json').write_text(json.dumps(site))
            with patch('tools.test.describe_pool', return_value=derived.copy()) as derive:
                self.assertEqual(verified_pool_site(folder, 'x86_64'), site)
                derive.assert_called_once_with('x86_64', target / 'BOOTX64.EFI', target / 'kernel.map')
            disk.write_bytes(b'changed disk bytes')
            with patch('tools.test.describe_pool', return_value=derived.copy()):
                with self.assertRaises(ValueError):
                    verified_pool_site(folder, 'x86_64')


@unittest.skipUnless(os.name == 'posix', 'runner uses POSIX pipes and signals')
class PoolTimeoutTests(unittest.TestCase):
    def test_pool_pass_followed_by_timeout_is_not_classified_as_success(self):
        with tempfile.TemporaryDirectory(prefix='styleos-pool-timeout-') as temporary:
            folder = Path(temporary)
            fake = folder / 'fake-qemu'
            fake.write_text(f'#!{sys.executable}\n'
                            'import os, signal, time\n'
                            'signal.alarm(5)\n'
                            'os.write(1, b"STYLEOS BOOTPOOL PASS\\n")\n'
                            'while True:\n    time.sleep(1)\n')
            fake.chmod(0o700)
            runner = Checks(folder)
            job = ('unit-pool-timeout', 'aarch64', str(fake), folder, None,
                   'bootpool', False, 128, 0.3)
            with patch('tools.test.serial_pool_ok', return_value=True) as classify:
                result = runner.run_qemu(job)
            self.assertEqual(result[1], 'FAIL', result)
            self.assertIn('timeout', result[2])
            classify.assert_not_called()


if __name__ == '__main__':
    unittest.main()
