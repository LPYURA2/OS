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


def build(arch, output, invalid=False, invalid_map=False, test_exception=False):
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    folder = output / arch
    folder.mkdir(parents=True, exist_ok=True)
    common = ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
              'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
              'kernel/exception.c']
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
    for rel in sources:
        source = ROOT / rel
        obj = folder / (rel.replace('/', '_').rsplit('.', 1)[0] + '.o')
        extra = ['-std=c11'] if source.suffix == '.c' else []
        run([cc, *flags, *extra, '-c', source, '-o', obj])
        objects.append(obj)
    if arch == 'x86_64':
        linker = tool('LLD_LINK', ['lld-link', 'lld-link-19', 'lld-link-18'])
        binary = folder / 'BOOTX64.EFI'
        map_options = ['/map:' + str(folder / 'kernel.map')] if test_exception else []
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
                        help='Default build/; test-exception defaults to build/exception')
    negative = parser.add_mutually_exclusive_group()
    negative.add_argument('--invalid-boot-info', action='store_true',
                        help='Negative test only: corrupt the handoff magic')
    negative.add_argument('--invalid-memory-map', action='store_true',
                          help='Negative test only: truncate the map input')
    negative.add_argument('--test-exception', action='store_true',
                          help='Dedicated image only: BRK / UD2 after valid handoff')
    args = parser.parse_args()
    output = (args.output or ROOT / 'build' /
              ('exception' if args.test_exception else '')).resolve()
    if args.test_exception and output == (ROOT / 'build').resolve():
        parser.error('--test-exception needs a separate directory, e.g. build/exception')
    arches = ['x86_64', 'aarch64'] if args.arch == 'all' else [args.arch]
    records = []
    for arch in arches:
        records.extend(build(arch, output, args.invalid_boot_info,
                             args.invalid_memory_map, args.test_exception))
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    (output / 'manifest.json').write_text(json.dumps({
        'prototype': 'StyleOS T-P1.2', 'compiler': version,
        'invalid_boot_info': args.invalid_boot_info,
        'invalid_memory_map': args.invalid_memory_map,
        'test_exception': args.test_exception, 'artifacts': records
    }, indent=2) + '\n')
    print('BUILD PASS:', ', '.join(arches))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.CalledProcessError) as error:
        print('BUILD FAIL:', error, file=sys.stderr)
        sys.exit(1)
