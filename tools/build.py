#!/usr/bin/env python3
"""Build real freestanding CPU binaries; no downloaded dependencies or root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def tool(variable, names):
    value = os.environ.get(variable)
    if value:
        found = shutil.which(value)
        if found:
            return found
        raise RuntimeError(f'{variable} is not executable: {value}')
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError(f'Missing {variable}: install {" / ".join(names)}')


def run(args):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, cwd=ROOT)


def build(arch, output, invalid=False, invalid_map=False, test_exception=False,
          test_bootpool=False, test_paging=False, test_paging_fault=False,
          test_ownership_plan=False, test_ownership_plan_invalid=False,
          test_tasks=False, test_user_processes=False, test_storage=False,
          test_disk_app=False, test_desktop_display=False, desktop_v0=False,
          test_paging_roundtrip=False, test_frame_grants=False, test_native_tea=False, test_native_tea_disk=False, test_native_tea_session=False, test_user_syscall=False, test_user_syscall_deny=False, test_user_ipc=False, test_user_ipc_deny=False, update_build=None, update_public_key=None, test_update_fail=False):
    if update_build is not None and (not desktop_v0 or arch != 'x86_64' or not update_public_key):
        raise ValueError('updatable guest requires x86-64 desktop and an Ed25519 public key')
    if test_user_ipc_deny: test_user_ipc=True
    if test_user_ipc:
        if arch != 'x86_64': raise ValueError('773 IPC hardware probe is x86-64 only')
        if test_user_syscall or test_user_syscall_deny or test_native_tea:
            raise ValueError('773 IPC and previous user tests cannot mix')
        test_user_processes=True
    if test_user_syscall_deny:
        test_user_syscall = True
    if test_user_syscall:
        if arch != 'x86_64': raise ValueError('772 hardware syscall profile is x86-64 only')
        test_user_processes = True
    if test_native_tea_session:
        test_native_tea_disk = True
    if test_native_tea_disk:
        test_native_tea = True
    if test_native_tea and arch != 'x86_64':
        raise ValueError('native .tea CPL3 test is x86-64 only')
    if test_native_tea:
        test_user_processes = True
    if desktop_v0 and arch != 'x86_64':
        raise ValueError('desktop v0 requires x86-64 UEFI')
    if test_desktop_display and arch != 'x86_64':
        raise ValueError('desktop display check supports only x86-64 UEFI')
    if test_storage and arch != 'aarch64':
        raise ValueError('storage test supports only AArch64 QEMU virt')
    if test_disk_app and arch != 'aarch64':
        raise ValueError('disk app test supports only AArch64 QEMU virt')
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    folder = output / arch
    folder.mkdir(parents=True, exist_ok=True)
    if test_native_tea:
        from build_native_tea_768 import build as build_tea
        build_tea(folder)
        if test_native_tea_disk:
            from build_native_tea_769 import build as build_disk_tea
            build_disk_tea(folder)
    common = ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
              'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
              'kernel/exception.c', 'kernel/boot_pool.c']
    if test_bootpool:
        common.append('kernel/boot_pool_selftest.c')
    if test_tasks:
        common += ['kernel/tasks.c','kernel/tasks_demo.c',
                   f'arch/{arch}/tasks.c',f'arch/{arch}/tasks_context.S']
    if test_user_processes:
        common += ['kernel/process_runtime.c', 'kernel/process_scheduler.c', 'kernel/user_processes.c', 'kernel/user_processes_demo.c',
                   f'arch/{arch}/user_processes.c', f'arch/{arch}/user_processes_entry.S']
        if test_user_syscall:
            common.append('kernel/user_copy_772.c')
        if test_user_ipc:
            common += ['kernel/user_copy_772.c','kernel/user_ipc_773.c',
                       'kernel/window_service.c','kernel/user_session_runtime.c',
                       'kernel/native_session_hw_770.c','kernel/native_window_channel.c']
        if test_native_tea_session:
            common += ['kernel/window_service.c', 'kernel/user_session_runtime.c',
                       'kernel/native_session_hw_770.c']
    if test_native_tea:
        common += ['kernel/native_tea_elf.c']
        if test_native_tea_disk:
            common += ['kernel/native_tea_package.c', 'kernel/tea_769_demo.c',
                       'arch/x86_64/tea_769_disk_io.c']
        else:
            common += ['kernel/tea_768_demo.c']
    if test_storage:
        common += ['kernel/storage.c', 'kernel/storage_demo.c',
                   'boot/aarch64/storage_fdt.c', 'arch/aarch64/virtio_block.c']
    if test_disk_app:
        common += ['kernel/storage.c', 'arch/aarch64/virtio_block.c',
                   'kernel/disk_app.c', 'kernel/disk_app_demo.c',
                   'boot/aarch64/disk_app_fdt.c', 'arch/aarch64/disk_app.c',
                   'arch/aarch64/disk_app_entry.S']
    if test_desktop_display or desktop_v0:
        common += ['kernel/desktop_display.c', 'boot/x86_64/desktop_display.c']
    if desktop_v0:
        common += ['kernel/process_runtime.c', 'kernel/process_scheduler.c', 'kernel/desktop_process_runtime.c', 'kernel/desktop_vm_stage.c', 'kernel/desktop_frame_owner.c', 'kernel/window_service.c', 'kernel/user_session_runtime.c', 'kernel/native_launch_stage.c', 'kernel/native_window_channel.c', 'kernel/process_inspector.c', 'kernel/process_view.c', 'kernel/process_api.c', 'kernel/system_apps.c', 'kernel/desktop_sessions.c', 'kernel/desktop_text_app.c', 'kernel/desktop_shell.c', 'kernel/desktop_draw.c',
                   'kernel/desktop_store.c', 'kernel/stylefs.c', 'kernel/desktop_volume_io.c',
                   'kernel/desktop_stylefs.c', 'kernel/desktop_extra_volume.c',
                   'arch/x86_64/desktop_io.c', 'arch/x86_64/desktop_process_arch.c']
    ownership = test_ownership_plan or test_ownership_plan_invalid
    if ownership:
        common += ['kernel/ownership.c', 'kernel/ownership_plan.c',
                   f'arch/{arch}/ownership.c']
    paging = test_paging or test_paging_fault or test_paging_roundtrip or test_frame_grants
    if paging:
        common += ['kernel/paging.c', 'kernel/paging_admission.c',
                   'kernel/paging_experiment.c', f'arch/{arch}/paging.c']
        if test_paging_fault:
            common.append(f'arch/{arch}/paging_fault.S')
    if test_frame_grants:
        common += ['kernel/frame_grant_proof.c', 'kernel/frame_grant_experiment.c',
                   'kernel/desktop_vm_stage.c']
    if (test_paging_roundtrip or test_frame_grants) and arch != 'x86_64':
        raise ValueError('CR3 roundtrip is x86-64 only')
    if arch == 'x86_64':
        target = 'x86_64-pc-win32-coff'
        sources = common + ['boot/x86_64/exit_boot.c', 'boot/x86_64/uefi.c',
                            'boot/x86_64/memory_map.c',
                            'arch/x86_64/serial.c', 'arch/x86_64/entry.S',
                            'arch/x86_64/exceptions.c', 'arch/x86_64/exception_entry.S']
        cpu_flags = ['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
        arch_id = '1'
    else:
        target = 'aarch64-none-elf'
        sources = common + ['boot/aarch64/platform.c', 'boot/aarch64/start.S',
                            'boot/aarch64/fdt_memory.c',
                            'arch/aarch64/console.c']
        # Early execution has the MMU off. Do not let Clang combine naturally
        # aligned fields into wider unaligned accesses to stack or DTB data.
        cpu_flags = ['-mgeneral-regs-only', '-mno-outline-atomics', '-mstrict-align']
        arch_id = '2'
    objects = []
    flags = ['--target=' + target, '-O2', '-g0', '-ffreestanding',
             '-fno-stack-protector', '-fno-builtin', '-fno-pic', '-fno-pie',
             '-fno-asynchronous-unwind-tables', '-fno-unwind-tables',
             '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'include'),
             '-I', str(ROOT / 'boot/x86_64'),
             '-DSTYLEOS_EXPECT_ARCH=' + arch_id,
             '-ffile-prefix-map=' + str(ROOT) + '=.'] + cpu_flags
    if invalid:
        flags.append('-DSTYLEOS_TEST_INVALID_BOOT_INFO=1')
    if invalid_map:
        flags.append('-DSTYLEOS_TEST_INVALID_MEMORY_MAP=1')
    if test_exception:
        flags.append('-DSTYLEOS_TEST_EXCEPTION=1')
    if test_bootpool:
        flags.append('-DSTYLEOS_TEST_BOOTPOOL=1')
    if paging:
        flags.append('-DSTYLEOS_TEST_PAGING=1')
    if test_paging_fault:
        flags.append('-DSTYLEOS_TEST_PAGING_FAULT=1')
    if test_paging_roundtrip or test_frame_grants:
        flags.append('-DSTYLEOS_TEST_PAGING_ROUNDTRIP=1')
    if test_frame_grants:
        flags.append('-DSTYLEOS_TEST_FRAME_GRANTS=1')
    if ownership:
        flags.append('-DSTYLEOS_TEST_OWNERSHIP=1')
    if test_ownership_plan_invalid:
        flags.append('-DSTYLEOS_TEST_OWNERSHIP_INVALID=1')
    if test_tasks:
        flags.append('-DSTYLEOS_TEST_TASKS=1')
    if test_user_processes:
        flags.append('-DSTYLEOS_TEST_USER_PROCESSES=1')
    if test_user_syscall:
        flags.append('-DSTYLEOS_TEST_USER_SYSCALL=1')
    if test_user_syscall_deny:
        flags.append('-DSTYLEOS_TEST_USER_SYSCALL_DENY=1')
    if test_user_ipc:
        flags.append('-DSTYLEOS_TEST_USER_IPC_773=1')
    if test_user_ipc_deny:
        flags.append('-DSTYLEOS_TEST_USER_IPC_773_DENY=1')
    if test_native_tea_disk:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA_DISK=1')
    if test_native_tea_session:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA_SESSION=1')
    if test_native_tea:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA=1')
        flags += ['-I', str(folder)]
    if test_storage:
        flags.append('-DSTYLEOS_TEST_STORAGE=1')
    if test_disk_app:
        flags.append('-DSTYLEOS_TEST_DISK_APP=1')
    if test_desktop_display:
        flags.append('-DSTYLEOS_TEST_DESKTOP_DISPLAY=1')
    if desktop_v0:
        flags.append('-DSTYLEOS_DESKTOP_V0=1')
    if update_build is not None:
        vendor = ROOT / 'third_party/monocypher'
        public = Path(update_public_key).read_bytes()
        if len(public) != 32 or not any(public):
            raise ValueError('expected a 32-byte raw Ed25519 public key')
        (folder / 'styleos_update_key.h').write_text(
            'const unsigned char styleos_update_key[32] = {' +
            ','.join(str(x) for x in public) + '};\n')
        flags += ['-DSTYLEOS_DESKTOP_UPDATE=1', f'-DSTYLEOS_UPDATE_BUILD={update_build}',
                  '-I', str(folder), '-I', str(vendor)]
        if test_update_fail:
            flags.append('-DSTYLEOS_TEST_UPDATE_FAIL=1')
        sources += ['kernel/system_update.c', 'kernel/desktop_import.c', 'boot/x86_64/desktop_update.c',
                    'third_party/monocypher/monocypher.c',
                    'third_party/monocypher/monocypher-ed25519.c']
    for rel in sources:
        source = ROOT / rel
        obj = folder / (rel.replace('/', '_').rsplit('.', 1)[0] + '.o')
        extra = ['-std=c11'] if source.suffix == '.c' else []
        run([cc, *flags, *extra, '-c', source, '-o', obj])
        objects.append(obj)
    if arch == 'x86_64':
        linker = tool('LLD_LINK', ['lld-link', 'lld-link-19', 'lld-link-18'])
        binary = folder / 'BOOTX64.EFI'
        map_options = ['/map:' + str(folder / 'kernel.map')]
        run([linker, '/subsystem:efi_application', '/entry:efi_main',
             '/machine:x64', '/nodefaultlib', '/timestamp:0', '/fixed:no',
             '/dynamicbase', '/nxcompat', '/out:' + str(binary), *map_options, *objects])
        run([sys.executable, ROOT / 'tools/make_disk.py', '--efi', binary,
             '--output', folder / 'styleos.img'])
        artifacts = [binary, folder / 'styleos.img']
    else:
        linker = tool('LD_LLD', ['ld.lld', 'ld.lld-19', 'ld.lld-18'])
        base = [linker, '-T', ROOT / 'boot/aarch64/linker.ld', '--build-id=none']
        run([*base, '-o', folder / 'kernel.elf', *objects])
        run([*base, '--oformat=binary', '-o', folder / 'styleos.bin', *objects])
        artifacts = [folder / 'kernel.elf', folder / 'styleos.bin']
    # Derive full-span placement from the linked image, never from a UART PASS.
    from pool_site import describe_pool
    pool_site = describe_pool(arch, artifacts[0],
                              folder / 'kernel.map' if arch == 'x86_64' else None,
                              folder / 'styleos.bin' if arch == 'aarch64' else None)
    if arch == 'x86_64':
        pool_site['disk_sha256'] = hashlib.sha256(
            (folder / 'styleos.img').read_bytes()).hexdigest()
    (folder / 'pool-site.json').write_text(json.dumps(pool_site, indent=2) + '\n')
    if test_tasks:
        from tasks_site import describe_tasks
        (folder / 'task-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources},indent=2)+'\n')
        site = describe_tasks(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            compiler_flags=flags)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'tasks-site.json').write_text(json.dumps(site,indent=2)+'\n')
    if test_storage:
        from storage_site import describe_storage
        (folder / 'storage-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_storage(arch, artifacts[0], raw_path=folder / 'styleos.bin',
                                compiler_flags=flags)
        (folder / 'storage-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_disk_app:
        from disk_app_site import describe_disk_app
        (folder / 'disk-app-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_disk_app(arch, artifacts[0], raw_path=folder / 'styleos.bin',
                                compiler_flags=flags)
        (folder / 'disk-app-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_user_processes and not test_native_tea:
        from user_processes_site import describe_user_processes
        (folder / 'user-processes-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_user_processes(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            compiler_flags=flags)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'user-processes-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if ownership:
        from ownership_site import describe_ownership
        site = describe_ownership(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            invalid=test_ownership_plan_invalid)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'ownership-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if paging:
        from paging_site import describe_paging
        site = describe_paging(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            fault=test_paging_fault)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'paging-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_exception:
        from fault_site import describe_fault
        site = describe_fault(arch, artifacts[0],
                              folder / 'kernel.map' if arch == 'x86_64' else None)
        site['binary_sha256'] = hashlib.sha256(artifacts[0].read_bytes()).hexdigest()
        if arch == 'aarch64':
            raw = (folder / 'styleos.bin').read_bytes()
            offset = site['image_offset']
            if raw[offset:offset + 4].hex() != site['instruction_hex']:
                raise ValueError('ELF fault symbol does not match raw ARM64 Image')
            site['raw_image_sha256'] = hashlib.sha256(raw).hexdigest()
        else:
            site['disk_sha256'] = hashlib.sha256((folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'fault-site.json').write_text(json.dumps(site, indent=2) + '\n')
    return [{'path': str(p.relative_to(output)), 'size': p.stat().st_size,
             'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in artifacts]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=['all', 'x86_64', 'aarch64'], default='all')
    parser.add_argument('--output', type=Path,
                        help='Default build/; dedicated test profiles use separate subdirectories')
    negative = parser.add_mutually_exclusive_group()
    negative.add_argument('--invalid-boot-info', action='store_true',
                        help='Negative test only: corrupt the handoff magic')
    negative.add_argument('--invalid-memory-map', action='store_true',
                          help='Negative test only: truncate the map input')
    negative.add_argument('--test-exception', action='store_true',
                          help='Dedicated image only: BRK / UD2 after valid handoff')
    negative.add_argument('--test-bootpool', action='store_true',
                          help='Dedicated image only: image-owned page pool write/read test')
    negative.add_argument('--test-paging', action='store_true',
                          help='Dedicated image only: owned translation tables and alias RW')
    negative.add_argument('--test-paging-fault', action='store_true',
                          help='Dedicated image only: owned tables and real unmapped read')
    negative.add_argument('--test-paging-roundtrip', action='store_true',
                          help='Dedicated x86-64 test only: prove new CR3 and restore inherited CR3')
    negative.add_argument('--test-frame-grants', action='store_true',
                          help='Dedicated x86-64 hardware proof of image-owned frame PA grants')
    negative.add_argument('--test-ownership-plan', action='store_true',
                          help='Dedicated image only: read-only ownership plan, GRANTED=0')
    negative.add_argument('--test-ownership-plan-invalid', action='store_true',
                          help='Dedicated image only: refuse an unproven mandatory claim')
    negative.add_argument('--test-tasks', action='store_true',
                          help='Dedicated image only: two cooperative integer kernel tasks')
    negative.add_argument('--test-user-processes', action='store_true',
                          help='Dedicated image only: CPL3/EL0 fault containment experiment')
    negative.add_argument('--test-user-syscall', action='store_true',
                          help='772 x86 CPL3 hardware user-copy trap positive proof')
    negative.add_argument('--test-user-ipc', action='store_true',
                          help='773 x86 CPL3 returning INT80 kernel-to-user window IPC guest')
    negative.add_argument('--test-user-ipc-deny', action='store_true',
                          help='773 deny foreign reply pointer before consuming window event')
    negative.add_argument('--test-user-syscall-deny', action='store_true',
                          help='772 x86 CPL3 hardware foreign-page refusal proof')
    negative.add_argument('--test-native-tea-session', action='store_true',
                          help='Hardware test only: real disk .tea PID/root bound to isolated session/window')
    negative.add_argument('--test-native-tea-disk', action='store_true',
                          help='Dedicated x86 CPL3 probe: guest reads canonical .tea from external read-only IDE')
    negative.add_argument('--test-native-tea', action='store_true',
                          help='Dedicated x86 CPL3 probe: ELF from a canonical build-verified .tea')
    negative.add_argument('--test-storage', action='store_true',
                          help='Dedicated AArch64 image only: persistent test file on VirtIO MMIO')
    negative.add_argument('--test-disk-app', action='store_true',
                          help='Dedicated AArch64 image only: load external disk app and execute EL0')
    negative.add_argument('--test-desktop-display', action='store_true',
                          help='Dedicated x86-64 UEFI image: framebuffer proof only')
    negative.add_argument('--desktop-v0', action='store_true',
                          help='Experimental interactive x86-64 UEFI desktop image')
    parser.add_argument('--update-build', type=int, help='Guest update build number')
    parser.add_argument('--update-public-key', type=Path, help='Raw Ed25519 verification key')
    parser.add_argument('--test-update-fail', action='store_true', help='Trial intentionally never becomes ready')
    args = parser.parse_args()
    if args.update_build is not None:
        if not (0 < args.update_build < 2**32 and args.desktop_v0 and args.arch == 'x86_64'
                and args.update_public_key and args.output):
            parser.error('--update-build needs x86-64 desktop, public key and dedicated --output')
        if args.output.resolve() in {(ROOT / 'build').resolve(), (ROOT / 'build/desktop-v0').resolve()}:
            parser.error('update builds must use a separate output folder')
    elif args.update_public_key or args.test_update_fail:
        parser.error('update key and test require --update-build')
    if (args.test_user_ipc or args.test_user_ipc_deny) and args.arch != 'x86_64':
        parser.error('773 returning IPC profile is x86-64 only')
    if (args.test_user_ipc or args.test_user_ipc_deny) and (args.test_user_syscall or args.test_user_syscall_deny):
        parser.error('772 and 773 trap profiles must be separate')
    if (args.test_user_syscall or args.test_user_syscall_deny) and args.arch != 'x86_64':
        parser.error('772 syscall profile is x86-64 only')
    if args.test_native_tea_session and args.arch != 'x86_64':
        parser.error('--test-native-tea-session requires --arch x86_64')
    if args.test_native_tea_disk and args.arch != 'x86_64':
        parser.error('--test-native-tea-disk requires --arch x86_64')
    if args.test_native_tea and args.arch != 'x86_64':
        parser.error('--test-native-tea requires --arch x86_64')
    if args.test_storage and args.arch != 'aarch64':
        parser.error('--test-storage requires --arch aarch64')
    if args.test_disk_app and args.arch != 'aarch64':
        parser.error('--test-disk-app requires --arch aarch64')
    if (args.test_paging_roundtrip or args.test_frame_grants) and args.arch != 'x86_64':
        parser.error('--test-paging-roundtrip requires --arch x86_64')
    if args.test_desktop_display and args.arch != 'x86_64':
        parser.error('--test-desktop-display requires --arch x86_64')
    if args.desktop_v0 and args.arch != 'x86_64':
        parser.error('--desktop-v0 requires --arch x86_64')
    output = (args.output or ROOT / 'build' /
              ('exception' if args.test_exception else
               'bootpool' if args.test_bootpool else
               'paging' if args.test_paging else
               'paging-fault' if args.test_paging_fault else
               'paging-roundtrip' if args.test_paging_roundtrip else
               'frame-grants' if args.test_frame_grants else
               'ownership' if args.test_ownership_plan else
               'ownership-invalid' if args.test_ownership_plan_invalid else
               'tasks' if args.test_tasks else
               'user-ipc-deny' if args.test_user_ipc_deny else
               'user-ipc' if args.test_user_ipc else
               'user-syscall-deny' if args.test_user_syscall_deny else
               'user-syscall' if args.test_user_syscall else
               'user-processes' if args.test_user_processes else
               'native-tea-session' if args.test_native_tea_session else
               'native-tea-disk' if args.test_native_tea_disk else
               'native-tea' if args.test_native_tea else
               'storage' if args.test_storage else
               'disk-app' if args.test_disk_app else
               'desktop-v0' if args.desktop_v0 else
               'desktop-display' if args.test_desktop_display else '')).resolve()
    if (args.test_exception or args.test_bootpool or args.test_paging or
            args.test_paging_fault or args.test_paging_roundtrip or args.test_frame_grants or args.test_ownership_plan or
            args.test_ownership_plan_invalid or args.test_tasks or
            args.test_user_processes or args.test_user_syscall or args.test_user_syscall_deny or args.test_user_ipc or args.test_user_ipc_deny or args.test_native_tea or args.test_native_tea_disk or args.test_native_tea_session or args.test_storage or args.test_disk_app or
            args.test_desktop_display or args.desktop_v0) and output == (ROOT / 'build').resolve():
        parser.error('dedicated tests need a separate directory, e.g. build/bootpool')
    arches = ['x86_64', 'aarch64'] if args.arch == 'all' else [args.arch]
    records = []
    for arch in arches:
        records.extend(build(arch, output, args.invalid_boot_info,
                             args.invalid_memory_map, args.test_exception,
                             args.test_bootpool, args.test_paging,
                             args.test_paging_fault, args.test_ownership_plan,
                             args.test_ownership_plan_invalid, args.test_tasks,
                             args.test_user_processes, args.test_storage,
                             args.test_disk_app, **({'test_paging_roundtrip': True} if args.test_paging_roundtrip else {}),
                             **({'test_frame_grants': True} if args.test_frame_grants else {}),
                             **({'test_user_ipc': True} if args.test_user_ipc else {}),
                             **({'test_user_ipc_deny': True} if args.test_user_ipc_deny else {}),
                             **({'test_user_syscall': True} if args.test_user_syscall else {}),
                             **({'test_user_syscall_deny': True} if args.test_user_syscall_deny else {}),
                             **({'test_native_tea': True} if args.test_native_tea else {}),
                             **({'test_native_tea_disk': True} if args.test_native_tea_disk else {}),
                             **({'test_native_tea_session': True} if args.test_native_tea_session else {}),
                             **({'test_desktop_display': True}
                                                   if args.test_desktop_display else {}),
                             **({'desktop_v0': True} if args.desktop_v0 else {}),
                             **({'update_build': args.update_build, 'update_public_key': args.update_public_key,
                                 'test_update_fail': args.test_update_fail} if args.update_build is not None else {})))
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    (output / 'manifest.json').write_text(json.dumps({
        'prototype': 'StyleOS guest updates v0.7' if args.update_build is not None else
                     'StyleOS desktop v0' if args.desktop_v0 else
                     'StyleOS desktop display check' if args.test_desktop_display else
                     'StyleOS T-P1.8B' if args.test_disk_app else
                     'StyleOS 770 guest disk .tea with PID/session/window proof' if args.test_native_tea_session else
                     'StyleOS 769 guest disk .tea CPL3 proof' if args.test_native_tea_disk else
                     'StyleOS 768 native .tea hardware proof' if args.test_native_tea else
                     'StyleOS T-P1.8A' if args.test_storage else
                     'StyleOS 773 x86 CPL3 returning window IPC' if (args.test_user_ipc or args.test_user_ipc_deny) else
                     'StyleOS 772 x86 CPL3 trap copy proof' if (args.test_user_syscall or args.test_user_syscall_deny) else
                     'StyleOS T-P1.7' if args.test_user_processes else 'StyleOS T-P1.6',
        **({'update_build': args.update_build, 'test_update_fail': args.test_update_fail}
           if args.update_build is not None else {}),
        'compiler': version,
        'invalid_boot_info': args.invalid_boot_info,
        'invalid_memory_map': args.invalid_memory_map,
        'test_exception': args.test_exception,
        'test_bootpool': args.test_bootpool,
        'test_paging': args.test_paging, 'test_paging_fault': args.test_paging_fault,
        'test_paging_roundtrip': args.test_paging_roundtrip or args.test_frame_grants,
        'test_frame_grants': args.test_frame_grants,
        'test_native_tea': args.test_native_tea or args.test_native_tea_disk,
        'test_native_tea_disk': args.test_native_tea_disk or args.test_native_tea_session,
        'test_native_tea_session': args.test_native_tea_session,
        'test_user_syscall': args.test_user_syscall or args.test_user_syscall_deny,
        'test_user_syscall_deny': args.test_user_syscall_deny,
        'test_user_ipc': args.test_user_ipc or args.test_user_ipc_deny,
        'test_user_ipc_deny': args.test_user_ipc_deny,
        'test_ownership_plan': args.test_ownership_plan,
        'test_ownership_plan_invalid': args.test_ownership_plan_invalid,
        'test_tasks': args.test_tasks,
        **({'test_desktop_display': True} if args.test_desktop_display else {}),
        **({'desktop_v0': True} if args.desktop_v0 else {}),
        **({'test_disk_app': True, 'test_storage': False, 'test_user_processes': False,
            'compiler_flags': {arch: json.loads((output / arch / 'disk-app-compiler.json').read_text())['flags']
                               for arch in arches},
            'disk_app_sources': {arch: json.loads((output / arch / 'disk-app-compiler.json').read_text())['sources']
                                 for arch in arches}} if args.test_disk_app else {}),
        **({'test_storage': True, 'test_user_processes': False,
            'compiler_flags': {arch: json.loads((output / arch / 'storage-compiler.json').read_text())['flags']
                               for arch in arches},
            'storage_sources': {arch: json.loads((output / arch / 'storage-compiler.json').read_text())['sources']
                                for arch in arches}} if args.test_storage else {}),
        **({'test_user_processes': True,
            'compiler_flags': {arch: json.loads((output / arch / 'user-processes-compiler.json').read_text())['flags']
                               for arch in arches},
            'user_processes_sources': {arch: json.loads((output / arch / 'user-processes-compiler.json').read_text())['sources']
                                      for arch in arches}} if args.test_user_processes else {}),
        **({'compiler_flags': {arch: json.loads((output / arch / 'task-compiler.json').read_text())['flags']
                              for arch in arches},
            'task_sources': {arch: json.loads((output / arch / 'task-compiler.json').read_text())['sources']
                             for arch in arches}} if args.test_tasks else {}),
        'artifacts': records
    }, indent=2) + '\n')
    print('BUILD PASS:', ', '.join(arches))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.CalledProcessError) as error:
        print('BUILD FAIL:', error, file=sys.stderr)
        sys.exit(1)
