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
         '--test-exception', '--test-bootpool', '--test-paging', '--test-paging-fault',
         '--test-ownership-plan', '--test-ownership-plan-invalid', '--test-tasks',
         '--test-user-processes', '--test-storage')


class BuildModeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='styleos-build-modes-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def invoke(self, arguments):
        def fake_build(_arch, output, *_modes):
            output.mkdir(parents=True, exist_ok=True)
            if any(_modes[-3:]):
                folder = output / _arch
                folder.mkdir(exist_ok=True)
                sidecar = ('storage-compiler.json' if _modes[-1] else
                           'user-processes-compiler.json' if _modes[-2] else 'task-compiler.json')
                (folder / sidecar).write_text(json.dumps({
                    'flags': ['fixture-flags'], 'sources': ['fixture-sources']}))
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

    def test_all_eleven_test_modes_are_pairwise_exclusive(self):
        for pair in combinations(MODES, 2):
            with self.subTest(pair=pair):
                status, compile_image, tool = self.invoke(list(pair))
                self.assertEqual(status, 2)
                compile_image.assert_not_called()
                tool.assert_not_called()

    def test_default_directories_separate_all_dedicated_images(self):
        for flag, suffix in ((None, ''), ('--test-exception', 'exception'),
                             ('--test-bootpool', 'bootpool'), ('--test-paging', 'paging'),
                             ('--test-paging-fault', 'paging-fault'),
                             ('--test-ownership-plan', 'ownership'),
                             ('--test-ownership-plan-invalid', 'ownership-invalid'),
                             ('--test-tasks', 'tasks'),
                             ('--test-user-processes', 'user-processes')):
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
                                      flag == '--test-paging-fault',
                                      flag == '--test-ownership-plan',
                                      flag == '--test-ownership-plan-invalid',
                                      flag == '--test-tasks',
                                      flag == '--test-user-processes', False))
                manifest = json.loads((output / 'manifest.json').read_text())
                self.assertEqual(manifest['test_exception'], flag == '--test-exception')
                self.assertEqual(manifest['test_bootpool'], flag == '--test-bootpool')
                self.assertEqual(manifest['test_paging'], flag == '--test-paging')
                self.assertEqual(manifest['test_paging_fault'], flag == '--test-paging-fault')
                self.assertEqual(manifest['test_ownership_plan'], flag == '--test-ownership-plan')
                self.assertEqual(manifest['test_ownership_plan_invalid'],
                                 flag == '--test-ownership-plan-invalid')
                self.assertEqual(manifest['test_tasks'], flag == '--test-tasks')
                self.assertEqual(manifest.get('test_user_processes', False), flag == '--test-user-processes')
                self.assertFalse(manifest.get('test_storage', False))

    def test_dedicated_images_reject_explicit_ordinary_output(self):
        for flag in MODES[2:]:
            for output in (self.root / 'build', self.root / 'build/../build'):
                with self.subTest(flag=flag, output=output):
                    arguments = [flag, '--output', str(output)]
                    if flag == '--test-storage':
                        arguments += ['--arch', 'aarch64']
                    status, compile_image, tool = self.invoke(arguments)
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
                    flag == '--test-paging', flag == '--test-paging-fault',
                    flag == '--test-ownership-plan', flag == '--test-ownership-plan-invalid',
                    flag == '--test-tasks', flag == '--test-user-processes',
                    flag == '--test-storage')

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
                    for path in ('kernel/ownership.c', 'kernel/ownership_plan.c',
                                 f'arch/{arch}/ownership.c'):
                        self.assertNotIn(ROOT / path, sources)
                    for cmd in compiles:
                        self.assertEqual('-DSTYLEOS_TEST_PAGING=1' in cmd, mode != 'normal')
                        self.assertEqual('-DSTYLEOS_TEST_PAGING_FAULT=1' in cmd,
                                         mode == 'paging-fault')
                        for define in ('EXCEPTION', 'BOOTPOOL', 'INVALID_BOOT_INFO',
                                       'INVALID_MEMORY_MAP', 'OWNERSHIP', 'OWNERSHIP_INVALID'):
                            self.assertNotIn(f'-DSTYLEOS_TEST_{define}=1', cmd)


    def test_ownership_code_and_refusal_only_enter_dedicated_profiles(self):
        for arch in ('x86_64', 'aarch64'):
            for mode in ('normal', 'ownership', 'ownership-invalid'):
                with self.subTest(arch=arch, mode=mode):
                    output = self.root / (arch + mode)
                    folder = output / arch
                    folder.mkdir(parents=True)
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(b'fixture command isolation bytes')
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'ownership_site': types.SimpleNamespace(
                                  describe_ownership=Mock(return_value={}))})):
                        build.build(arch, output, test_ownership_plan=mode == 'ownership',
                                    test_ownership_plan_invalid=mode == 'ownership-invalid')
                    compiles = [call.args[0] for call in commands.call_args_list
                                if '-c' in call.args[0]]
                    sources = [Path(cmd[cmd.index('-c') + 1]) for cmd in compiles]
                    for path in ('kernel/ownership.c', 'kernel/ownership_plan.c',
                                 f'arch/{arch}/ownership.c'):
                        self.assertEqual(ROOT / path in sources, mode != 'normal')
                    for path in ('kernel/paging.c', 'kernel/paging_admission.c',
                                 'kernel/paging_experiment.c', 'kernel/boot_pool_selftest.c',
                                 f'arch/{arch}/paging.c', f'arch/{arch}/paging_fault.S'):
                        self.assertNotIn(ROOT / path, sources)
                    for cmd in compiles:
                        self.assertEqual('-DSTYLEOS_TEST_OWNERSHIP=1' in cmd,
                                         mode != 'normal')
                        self.assertEqual('-DSTYLEOS_TEST_OWNERSHIP_INVALID=1' in cmd,
                                         mode == 'ownership-invalid')
                        for define in ('EXCEPTION', 'BOOTPOOL', 'PAGING', 'PAGING_FAULT',
                                       'INVALID_BOOT_INFO', 'INVALID_MEMORY_MAP'):
                            self.assertNotIn(f'-DSTYLEOS_TEST_{define}=1', cmd)


    def test_every_legacy_profile_excludes_ownership_code_and_defines(self):
        for arch in ('x86_64', 'aarch64'):
            for index in range(7):
                with self.subTest(arch=arch, legacy_mode=index):
                    output = self.root / f'legacy-{arch}-{index}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    data = b'fixture static bytes'
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(data)
                    modes = [False] * 6
                    if index:
                        modes[index - 1] = True
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'paging_site': types.SimpleNamespace(describe_paging=Mock(return_value={})),
                              'fault_site': types.SimpleNamespace(describe_fault=Mock(return_value={
                                  'image_offset': 0, 'instruction_hex': data[:4].hex()}))})):
                        build.build(arch, output, *modes)
                    for call in commands.call_args_list:
                        cmd = call.args[0]
                        if '-c' not in cmd:
                            continue
                        self.assertNotIn('ownership', str(cmd[cmd.index('-c') + 1]))
                        self.assertFalse(any('STYLEOS_TEST_OWNERSHIP' in str(arg) for arg in cmd))


    def test_tasks_code_and_integer_abi_only_enter_the_dedicated_profile(self):
        sources = ('kernel/tasks.c', 'kernel/tasks_demo.c',
                   'arch/{arch}/tasks.c', 'arch/{arch}/tasks_context.S')
        for arch in ('x86_64', 'aarch64'):
            for active in (False, True):
                with self.subTest(arch=arch, tasks=active):
                    output = self.root / f'tasks-{arch}-{active}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(b'fixture task command bytes')
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'tasks_site': types.SimpleNamespace(
                                  describe_tasks=Mock(return_value={})),
                              'pool_site': types.SimpleNamespace(
                                  describe_pool=Mock(return_value={}))})):
                        build.build(arch, output, test_tasks=active)
                    compiles = [call.args[0] for call in commands.call_args_list
                                if '-c' in call.args[0]]
                    compiled = [Path(cmd[cmd.index('-c') + 1]) for cmd in compiles]
                    for path in sources:
                        self.assertEqual(ROOT / path.format(arch=arch) in compiled, active)
                    for cmd in compiles:
                        self.assertEqual('-DSTYLEOS_TEST_TASKS=1' in cmd, active)
                        for define in ('EXCEPTION', 'BOOTPOOL', 'PAGING', 'PAGING_FAULT',
                                       'OWNERSHIP', 'OWNERSHIP_INVALID',
                                       'INVALID_BOOT_INFO', 'INVALID_MEMORY_MAP'):
                            self.assertNotIn(f'-DSTYLEOS_TEST_{define}=1', cmd)

    def test_every_old_profile_excludes_tasks_sources_and_define(self):
        for arch in ('x86_64', 'aarch64'):
            for index in range(9):
                with self.subTest(arch=arch, old_mode=index):
                    output = self.root / f'old-tasks-{arch}-{index}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    data = b'fixture old profile image bytes'
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(data)
                    modes = [False] * 8
                    if index:
                        modes[index - 1] = True
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'paging_site': types.SimpleNamespace(describe_paging=Mock(return_value={})),
                              'ownership_site': types.SimpleNamespace(describe_ownership=Mock(return_value={})),
                              'fault_site': types.SimpleNamespace(describe_fault=Mock(return_value={
                                  'image_offset': 0, 'instruction_hex': data[:4].hex()}))})):
                        build.build(arch, output, *modes)
                    for call in commands.call_args_list:
                        cmd = call.args[0]
                        if '-c' in cmd:
                            self.assertNotIn('tasks', str(cmd[cmd.index('-c') + 1]))
                            self.assertNotIn('-DSTYLEOS_TEST_TASKS=1', cmd)


    def test_user_process_sources_only_enter_the_new_profile(self):
        expected = ('kernel/user_processes.c', 'kernel/user_processes_demo.c',
                    'arch/{arch}/user_processes.c', 'arch/{arch}/user_processes_entry.S')
        for arch in ('x86_64', 'aarch64'):
            for active in (False, True):
                with self.subTest(arch=arch, user_processes=active):
                    output = self.root / f'user-{arch}-{active}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(b'fixture user command bytes')
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'user_processes_site': types.SimpleNamespace(
                                  describe_user_processes=Mock(return_value={}))})):
                        build.build(arch, output, test_user_processes=active)
                    compiles = [call.args[0] for call in commands.call_args_list
                                if '-c' in call.args[0]]
                    compiled = [Path(cmd[cmd.index('-c') + 1]) for cmd in compiles]
                    for path in expected:
                        self.assertEqual(ROOT / path.format(arch=arch) in compiled, active)
                    for cmd in compiles:
                        self.assertEqual('-DSTYLEOS_TEST_USER_PROCESSES=1' in cmd, active)
                        for define in ('EXCEPTION', 'BOOTPOOL', 'PAGING', 'PAGING_FAULT',
                                       'OWNERSHIP', 'OWNERSHIP_INVALID', 'TASKS',
                                       'INVALID_BOOT_INFO', 'INVALID_MEMORY_MAP'):
                            self.assertNotIn(f'-DSTYLEOS_TEST_{define}=1', cmd)

    def test_all_ten_base_profiles_exclude_user_process_sources_and_define(self):
        for arch in ('x86_64', 'aarch64'):
            for index in range(10):
                with self.subTest(arch=arch, base_mode=index):
                    output = self.root / f'base-user-{arch}-{index}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    data = b'fixture base bytes'
                    for artifact in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                     else ('kernel.elf', 'styleos.bin')):
                        (folder / artifact).write_bytes(data)
                    modes = [False] * 9
                    if index:
                        modes[index - 1] = True
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'paging_site': types.SimpleNamespace(describe_paging=Mock(return_value={})),
                              'ownership_site': types.SimpleNamespace(describe_ownership=Mock(return_value={})),
                              'tasks_site': types.SimpleNamespace(describe_tasks=Mock(return_value={})),
                              'fault_site': types.SimpleNamespace(describe_fault=Mock(return_value={
                                  'image_offset': 0, 'instruction_hex': data[:4].hex()}))})):
                        build.build(arch, output, *modes)
                    for call in commands.call_args_list:
                        cmd = call.args[0]
                        if '-c' in cmd:
                            self.assertNotIn('user_processes', str(cmd[cmd.index('-c') + 1]))
                            self.assertNotIn('-DSTYLEOS_TEST_USER_PROCESSES=1', cmd)


    def test_storage_requires_arm_and_has_its_own_default_directory(self):
        for arch in ('all', 'x86_64'):
            status, compile_image, compiler = self.invoke(['--test-storage', '--arch', arch])
            self.assertEqual(status, 2)
            compile_image.assert_not_called()
            compiler.assert_not_called()
        status, compile_image, _ = self.invoke(['--test-storage', '--arch', 'aarch64'])
        self.assertEqual(status, 0)
        output = (self.root / 'build/storage').resolve()
        compile_image.assert_called_once_with('aarch64', output, *([False] * 10), True)
        manifest = json.loads((output / 'manifest.json').read_text())
        self.assertTrue(manifest['test_storage'])
        self.assertFalse(manifest['test_user_processes'])
        self.assertIn('storage_sources', manifest)

    def test_direct_storage_builder_rejects_x86_before_using_tools(self):
        with patch.object(build, 'tool') as compiler:
            with self.assertRaisesRegex(ValueError, 'only AArch64'):
                build.build('x86_64', self.root / 'refused', test_storage=True)
            compiler.assert_not_called()
        self.assertFalse((self.root / 'refused').exists())

    def test_storage_sources_only_enter_the_arm_storage_image(self):
        expected = ('kernel/storage.c', 'kernel/storage_demo.c',
                    'boot/aarch64/storage_fdt.c', 'arch/aarch64/virtio_block.c')
        for active in (False, True):
            output = self.root / f'storage-{active}'
            folder = output / 'aarch64'
            folder.mkdir(parents=True)
            for name in ('kernel.elf', 'styleos.bin'):
                (folder / name).write_bytes(b'fixture storage command isolation')
            with (patch.object(build, 'tool', return_value='/fake/tool'),
                  patch.object(build, 'run') as commands,
                  patch.dict(sys.modules, {
                      'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                      'storage_site': types.SimpleNamespace(describe_storage=Mock(return_value={}))})):
                build.build('aarch64', output, test_storage=active)
            compiled = [call.args[0] for call in commands.call_args_list if '-c' in call.args[0]]
            for source in expected:
                self.assertEqual(ROOT / source in [Path(cmd[cmd.index('-c') + 1])
                                                  for cmd in compiled], active)
            for cmd in compiled:
                self.assertEqual('-DSTYLEOS_TEST_STORAGE=1' in cmd, active)
                self.assertFalse(any(isinstance(arg, str) and arg.startswith('-DSTYLEOS_TEST_') and
                                     arg != '-DSTYLEOS_TEST_STORAGE=1' for arg in cmd))

    def test_all_eleven_base_profiles_exclude_storage_sources_and_define(self):
        for arch in ('x86_64', 'aarch64'):
            for index in range(11):
                with self.subTest(arch=arch, base_mode=index):
                    output = self.root / f'base-storage-{arch}-{index}'
                    folder = output / arch
                    folder.mkdir(parents=True)
                    data = b'fixture base storage isolation bytes'
                    for name in (('BOOTX64.EFI', 'styleos.img') if arch == 'x86_64'
                                 else ('kernel.elf', 'styleos.bin')):
                        (folder / name).write_bytes(data)
                    modes = [False] * 10
                    if index:
                        modes[index - 1] = True
                    with (patch.object(build, 'tool', return_value='/fake/tool'),
                          patch.object(build, 'run') as commands,
                          patch.dict(sys.modules, {
                              'pool_site': types.SimpleNamespace(describe_pool=Mock(return_value={})),
                              'paging_site': types.SimpleNamespace(describe_paging=Mock(return_value={})),
                              'ownership_site': types.SimpleNamespace(describe_ownership=Mock(return_value={})),
                              'tasks_site': types.SimpleNamespace(describe_tasks=Mock(return_value={})),
                              'user_processes_site': types.SimpleNamespace(describe_user_processes=Mock(return_value={})),
                              'fault_site': types.SimpleNamespace(describe_fault=Mock(return_value={
                                  'image_offset': 0, 'instruction_hex': data[:4].hex()}))})):
                        build.build(arch, output, *modes)
                    for call in commands.call_args_list:
                        cmd = call.args[0]
                        if '-c' not in cmd:
                            continue
                        source = Path(cmd[cmd.index('-c') + 1])
                        self.assertNotIn(source, [ROOT / name for name in (
                            'kernel/storage.c', 'kernel/storage_demo.c',
                            'boot/aarch64/storage_fdt.c', 'arch/aarch64/virtio_block.c')])
                        self.assertNotIn('-DSTYLEOS_TEST_STORAGE=1', cmd)


if __name__ == '__main__':
    unittest.main()
