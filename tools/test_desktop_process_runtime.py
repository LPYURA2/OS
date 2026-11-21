#!/usr/bin/env python3
"""Sanitized host + UEFI COFF compile of native desktop process service."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / p for p in (
    'kernel/process_runtime.c', 'kernel/process_scheduler.c',
    'kernel/desktop_process_runtime.c', 'kernel/window_service.c',
    'kernel/process_inspector.c', 'kernel/process_view.c',
    'kernel/process_api.c', 'tests/test_desktop_process_runtime.c')]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT / 'include')]
    exe = args.output / 'desktop-process-runtime-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        *inc, *map(str, SOURCES), '-o', str(exe)], check=True)
    test = subprocess.run([str(exe)], text=True, capture_output=True)
    (args.output / 'test.log').write_text(test.stdout + test.stderr)
    print(test.stdout, end='')
    if test.returncode:
        print(test.stderr, end='')
        raise SystemExit(test.returncode)
    for name in ('kernel/desktop_process_runtime.c',
                 'arch/x86_64/desktop_process_arch.c'):
        subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-std=c11',
            '-O2', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
            '-Wall', '-Wextra', '-Werror', '-mno-red-zone', '-mno-sse',
            '-mno-mmx', '-msoft-float', *inc, '-c', str(ROOT / name),
            '-o', str(args.output / (Path(name).stem + '.obj'))], check=True)
    print('DESKTOP PROCESS RUNTIME UEFI COFF PASS')

if __name__ == '__main__':
    main()
