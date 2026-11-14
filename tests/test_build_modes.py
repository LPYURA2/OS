"""Build-mode isolation without invoking a compiler or creating real images."""
from contextlib import redirect_stderr, redirect_stdout
from itertools import combinations
from pathlib import Path
import io
import json
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import build


MODES = ('--invalid-boot-info', '--invalid-memory-map',
         '--test-exception', '--test-bootpool', '--test-paging', '--test-paging-fault')


class BuildModeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='styleos-build-modes-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def invoke(self, arguments):
        def fake_build(_arch, output, *_modes):
            output.mkdir(parents=True, exist_ok=True)
            return []

        with (patch.object(build, 'ROOT', self.root),
              patch.object(sys, 'argv', ['build.py', *arguments]),
              patch.object(build, 'build', side_effect=fake_build) as compile_image,
              patch.object(build, 'tool', return_value='/fake/clang') as tool,
              patch.object(build.subprocess, 'check_output', return_value='fake clang\n'),
              redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO())):
            try:
                build.main()
                status = 0
            except SystemExit as error:
                status = error.code
        return status, compile_image, tool

    def test_all_six_test_modes_are_pairwise_exclusive(self):
        for pair in combinations(MODES, 2):
            with self.subTest(pair=pair):
                status, compile_image, tool = self.invoke(list(pair))
                self.assertEqual(status, 2)
                compile_image.assert_not_called()
                tool.assert_not_called()

    def test_default_directories_separate_both_dedicated_images(self):
        for flag, suffix in ((None, ''), ('--test-exception', 'exception'),
                             ('--test-bootpool', 'bootpool'), ('--test-paging', 'paging'),
                             ('--test-paging-fault', 'paging-fault')):
            with self.subTest(flag=flag):
                status, compile_image, _ = self.invoke([] if flag is None else [flag])
                self.assertEqual(status, 0)
                self.assertEqual(compile_image.call_count, 2)
                output = (self.root / 'build' / suffix).resolve()
                self.assertEqual([call.args[0] for call in compile_image.call_args_list],
                                 ['x86_64', 'aarch64'])
                for call in compile_image.call_args_list:
                    self.assertEqual(call.args[1], output)
                    self.assertEqual(call.args[2:],
                                     (False, False, flag == '--test-exception',
                                      flag == '--test-bootpool', flag == '--test-paging',
                                      flag == '--test-paging-fault'))
                manifest = json.loads((output / 'manifest.json').read_text())
                self.assertEqual(manifest['test_exception'], flag == '--test-exception')
                self.assertEqual(manifest['test_bootpool'], flag == '--test-bootpool')
                self.assertEqual(manifest['test_paging'], flag == '--test-paging')
                self.assertEqual(manifest['test_paging_fault'], flag == '--test-paging-fault')

    def test_dedicated_images_reject_explicit_ordinary_output(self):
        for flag in MODES[2:]:
            for output in (self.root / 'build', self.root / 'build/../build'):
                with self.subTest(flag=flag, output=output):
                    status, compile_image, tool = self.invoke([flag, '--output', str(output)])
                    self.assertEqual(status, 2)
                    compile_image.assert_not_called()
                    tool.assert_not_called()

    def test_custom_dedicated_output_preserves_requested_arch_and_mode(self):
        for flag in MODES[2:]:
            with self.subTest(flag=flag):
                output = self.root / ('custom-' + flag.removeprefix('--'))
                status, compile_image, _ = self.invoke([
                    '--arch', 'aarch64', flag, '--output', str(output)])
                self.assertEqual(status, 0)
                compile_image.assert_called_once_with('aarch64', output.resolve(),
                    False, False, flag == '--test-exception', flag == '--test-bootpool',
                    flag == '--test-paging', flag == '--test-paging-fault')

    def test_page_writes_and_test_define_only_enter_bootpool_images(self):
        for arch in ('x86_64', 'aarch64'):
            for test_bootpool in (False, True):
                with self.subTest(arch=arch, test_bootpool=test_bootpool):
                    output = self.root / (arch + str(test_bootpool))
                    folder = output / arch
                    folder.mkdir(parents=True)
                    artifacts = ('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64' else (
                        'kernel.elf', 'styleos.bin')
                    for artifact in artifacts:
                        (folder / artifact).write_bytes(b'fake image for source-list test')
                    describe_pool = Mock(return_value={})
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {'pool_site': types.SimpleNamespace(
                              describe_pool=describe_pool)})):
                        build.build(arch, output, test_bootpool=test_bootpool)
                    compiler_commands = [call.args[0] for call in commands.call_args_list
                                         if '-c' in call.args[0]]
                    sources = [Path(command[command.index('-c') + 1])
                               for command in compiler_commands]
                    self.assertIn(ROOT / 'kernel/boot_pool.c', sources)
                    self.assertEqual(ROOT / 'kernel/boot_pool_selftest.c' in sources,
                                     test_bootpool)
                    for command in compiler_commands:
                        self.assertEqual('-DSTYLEOS_TEST_BOOTPOOL=1' in command, test_bootpool)
                        self.assertNotIn('-DSTYLEOS_TEST_EXCEPTION=1', command)
                        self.assertNotIn('-DSTYLEOS_TEST_INVALID_BOOT_INFO=1', command)
                        self.assertNotIn('-DSTYLEOS_TEST_INVALID_MEMORY_MAP=1', command)

    def test_paging_reserve_and_injection_only_enter_their_dedicated_images(self):
        for arch in ('x86_64', 'aarch64'):
            for mode in ('normal', 'paging', 'paging-fault'):
                with self.subTest(arch=arch, mode=mode):
                    output = self.root / (arch + mode)
                    folder = output / arch
                    folder.mkdir(parents=True)
                    artifacts = ('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64' else (
                        'kernel.elf', 'styleos.bin')
                    for artifact in artifacts:
                        (folder / artifact).write_bytes(b'fake bytes for command isolation test')
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'paging_site': types.SimpleNamespace(describe_paging=Mock(return_value={}))})):
                        build.build(arch, output, test_paging=mode == 'paging',
                                    test_paging_fault=mode == 'paging-fault')
                    compiles = [call.args[0] for call in commands.call_args_list
                                if '-c' in call.args[0]]
                    sources = [Path(cmd[cmd.index('-c') + 1]) for cmd in compiles]
                    for path in ('kernel/paging.c', 'kernel/paging_admission.c',
                                 'kernel/paging_experiment.c', f'arch/{arch}/paging.c'):
                        self.assertEqual(ROOT / path in sources, mode != 'normal')
                    self.assertEqual(ROOT / f'arch/{arch}/paging_fault.S' in sources,
                                     mode == 'paging-fault')
                    self.assertNotIn(ROOT / 'kernel/boot_pool_selftest.c', sources)
                    for cmd in compiles:
                        self.assertEqual('-DSTYLEOS_TEST_PAGING=1' in cmd, mode != 'normal')
                        self.assertEqual('-DSTYLEOS_TEST_PAGING_FAULT=1' in cmd,
                                         mode == 'paging-fault')
                        for define in ('EXCEPTION', 'BOOTPOOL', 'INVALID_BOOT_INFO',
                                       'INVALID_MEMORY_MAP'):
                            self.assertNotIn(f'-DSTYLEOS_TEST_{define}=1', cmd)


if __name__ == '__main__':
    unittest.main()
