#!/usr/bin/env python3
"""Sanitized host test and freestanding cross-compilation for bounded dispatcher."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    opts = p.parse_args()
    opts.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT / 'include')]
    sources = [str(ROOT / s) for s in (
        'kernel/process_runtime.c', 'kernel/process_scheduler.c',
        'tests/test_process_scheduler.c')]
    exe = opts.output / 'process-scheduler-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        *inc, *sources, '-o', str(exe)], check=True)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (opts.output / 'test.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr, end='')
        raise SystemExit(run.returncode)
    for target in ('x86_64-pc-win32-coff', 'aarch64-none-elf'):
        flags = (['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
            if target.startswith('x86') else
            ['-mgeneral-regs-only', '-mno-outline-atomics', '-mstrict-align'])
        subprocess.run([cc, '--target=' + target, '-std=c11', '-O2',
            '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
            '-Wall', '-Wextra', '-Werror', *flags, *inc, '-c',
            str(ROOT / 'kernel/process_scheduler.c'), '-o',
            str(opts.output / (target + '.o'))], check=True)
    print('PROCESS SCHEDULER COFF/AARCH64 PASS')

if __name__ == '__main__':
    main()
