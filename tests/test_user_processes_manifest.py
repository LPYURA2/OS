"""Bind process-profile manifests to independently parsed PE/ELF byte fixtures.

Only LLVM instruction decoding is replaced here. Image headers, symbols,
full resource extents, raw ARM Image, program bytes, sidecar comparison and
artifact files all pass through the actual production verifier.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.integer_code import _flags
from tools.test import _user_processes_sources, verified_user_processes_site
from tools.user_processes_site import describe_user_processes
from test_user_processes_site import arm_fixture, pe_fixture, fixture_decoder


MODES = dict(invalid_boot_info=False, invalid_memory_map=False,
             test_exception=False, test_bootpool=False, test_paging=False,
             test_paging_fault=False, test_ownership_plan=False,
             test_ownership_plan_invalid=False, test_tasks=False,
             test_user_processes=True)


def decode_fixture(arch, binary, spans, flags):
    proof, decoded = fixture_decoder(arch, binary, spans, flags)
    proof['compiler_flags'] = _flags(arch, flags)
    return proof, decoded


class UserProcessManifestTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='styleos-user-manifest-')
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)
        decoder = patch('tools.user_processes_site.verify_integer_code',
                        side_effect=decode_fixture)
        decoder.start()
        self.addCleanup(decoder.stop)

    def fixture(self, arch):
        folder = self.folder / arch
        folder.mkdir()
        target = folder / arch
        target.mkdir()
        flags = (['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
                 if arch == 'x86_64' else ['-mgeneral-regs-only'])
        flags += ['--target=' + ('x86_64-pc-win32-coff' if arch == 'x86_64'
                                else 'aarch64-none-elf'),
                  '-DSTYLEOS_TEST_USER_PROCESSES=1']
        sources = _user_processes_sources(arch)
        if arch == 'x86_64':
            binary, symbols = pe_fixture()
            (target / 'BOOTX64.EFI').write_bytes(binary)
            (target / 'kernel.map').write_text(symbols)
            # Disk contents are real bytes whose size and hash are bound below;
            # filesystem packaging is checked by the existing disk tests.
            (target / 'styleos.img').write_bytes(b'disk-fixture\0' + binary)
            site = describe_user_processes(arch, target / 'BOOTX64.EFI',
                target / 'kernel.map', compiler_flags=flags)
            site['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
            names = [('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')]
        else:
            binary, raw, _ = arm_fixture()
            (target / 'kernel.elf').write_bytes(binary)
            (target / 'styleos.bin').write_bytes(raw)
            site = describe_user_processes(arch, target / 'kernel.elf',
                raw_path=target / 'styleos.bin', compiler_flags=flags)
            names = [('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')]
        compiler = dict(flags=flags, sources=sources)
        manifest = dict(MODES, compiler_flags={arch: flags},
                        user_processes_sources={arch: sources}, artifacts=[])
        for name, key in names:
            manifest['artifacts'].append(dict(path=arch + '/' + name,
                size=(target / name).stat().st_size, sha256=site[key]))
        self.write(folder / 'manifest.json', manifest)
        self.write(target / 'user-processes-compiler.json', compiler)
        self.write(target / 'user-processes-site.json', site)
        self.assertEqual(verified_user_processes_site(folder, arch), site)
        return folder, target, manifest, compiler, site

    @staticmethod
    def write(path, value):
        path.write_text(json.dumps(value))

    def test_every_mode_is_an_exact_bool_and_dedicated_profile_is_required(self):
        for arch in ('x86_64', 'aarch64'):
            folder, _, baseline, _, _ = self.fixture(arch)
            for key, original in MODES.items():
                for value in (not original, int(original), None):
                    with self.subTest(arch=arch, mode=key, value=value):
                        changed = deepcopy(baseline)
                        changed[key] = value
                        self.write(folder / 'manifest.json', changed)
                        with self.assertRaisesRegex(ValueError, 'manifest mode'):
                            verified_user_processes_site(folder, arch)

    def test_flags_require_arch_map_nonempty_strings_target_and_only_new_define(self):
        for arch in ('x86_64', 'aarch64'):
            folder, _, baseline, _, _ = self.fixture(arch)
            original = baseline['compiler_flags'][arch]
            candidates = [None, [], 'bad', {}, {arch: []}, {arch: 'bad'},
                          {arch: [True]}, {arch: original[:-1]},
                          {arch: original + ['-DSTYLEOS_TEST_TASKS=1']},
                          {arch: original + ['-DSTYLEOS_TEST_USER_PROCESSES=1']},
                          {arch: [flag for flag in original if not flag.startswith('--target=')]}]
            for value in candidates:
                with self.subTest(arch=arch, flags=value):
                    changed = deepcopy(baseline)
                    changed['compiler_flags'] = value
                    self.write(folder / 'manifest.json', changed)
                    with self.assertRaises(ValueError):
                        verified_user_processes_site(folder, arch)

    def test_sources_require_all_profile_units_without_duplicates_or_foreign_units(self):
        for arch in ('x86_64', 'aarch64'):
            folder, _, baseline, _, _ = self.fixture(arch)
            original = baseline['user_processes_sources'][arch]
            candidates = [None, [], 'bad', {}, {arch: []}, {arch: original[:-1]},
                          {arch: original + [original[0]]},
                          {arch: ['kernel/ownership.c', *original[1:]]},
                          {arch: [None, *original[1:]]},
                          {arch: [{}, *original[1:]]}]
            for value in candidates:
                with self.subTest(arch=arch, sources=value):
                    changed = deepcopy(baseline)
                    changed['user_processes_sources'] = value
                    self.write(folder / 'manifest.json', changed)
                    with self.assertRaises(ValueError):
                        verified_user_processes_site(folder, arch)

    def test_compiler_and_site_sidecars_cannot_replace_independent_recomputation(self):
        for arch in ('x86_64', 'aarch64'):
            folder, target, _, compiler, site = self.fixture(arch)
            for value in (None, {}, dict(compiler, flags=compiler['flags'][:-1]),
                          dict(compiler, sources=compiler['sources'][:-1])):
                with self.subTest(arch=arch, compiler=value):
                    self.write(target / 'user-processes-compiler.json', value)
                    with self.assertRaisesRegex(ValueError, 'compiler sidecar'):
                        verified_user_processes_site(folder, arch)
            self.write(target / 'user-processes-compiler.json', compiler)
            for change in (lambda d: d.update(binary_sha256='0' * 64),
                           lambda d: d['objects']['control'].update(bytes=4096),
                           lambda d: d['programs']['A']['sites']['fault'].update(user_pc=0x10000000)):
                value = deepcopy(site)
                change(value)
                self.write(target / 'user-processes-site.json', value)
                with self.assertRaisesRegex(ValueError, 'site differs'):
                    verified_user_processes_site(folder, arch)

    def test_artifact_count_path_size_and_sha_are_bound_for_both_isas(self):
        for arch in ('x86_64', 'aarch64'):
            folder, _, baseline, _, _ = self.fixture(arch)
            for index in (0, 1):
                for key, value in [('size', True), ('size', 1), ('sha256', '0' * 64),
                                   ('path', arch + '/different.bin')]:
                    with self.subTest(arch=arch, index=index, key=key):
                        changed = deepcopy(baseline)
                        changed['artifacts'][index][key] = value
                        self.write(folder / 'manifest.json', changed)
                        with self.assertRaises(ValueError):
                            verified_user_processes_site(folder, arch)
            for artifacts in (None, [], baseline['artifacts'][:-1],
                              baseline['artifacts'] + [baseline['artifacts'][0]],
                              [None, baseline['artifacts'][1]]):
                changed = deepcopy(baseline)
                changed['artifacts'] = artifacts
                self.write(folder / 'manifest.json', changed)
                with self.subTest(arch=arch, artifacts=artifacts), self.assertRaises(ValueError):
                    verified_user_processes_site(folder, arch)

    def test_file_tampering_is_refused_even_with_untouched_manifest_and_sidecars(self):
        for arch in ('x86_64', 'aarch64'):
            folder, target, baseline, _, _ = self.fixture(arch)
            for artifact in baseline['artifacts']:
                path = target / Path(artifact['path']).name
                original = path.read_bytes()
                path.write_bytes(original + b'tamper')
                with self.subTest(arch=arch, artifact=path.name), self.assertRaises(ValueError):
                    verified_user_processes_site(folder, arch)
                path.write_bytes(original)


if __name__ == '__main__':
    unittest.main()
