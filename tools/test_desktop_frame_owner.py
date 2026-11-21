#!/usr/bin/env python3
"""Sanitized host test + x86 UEFI COFF compilation for detached frame leases."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT / 'include')]
    names = ['kernel/desktop_vm_stage.c', 'kernel/desktop_frame_owner.c',
             'kernel/process_runtime.c', 'tests/test_desktop_frame_owner.c']
    exe = a.output / 'desktop-frame-owner-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', *inc,
        *(str(ROOT / n) for n in names), '-o', str(exe)], check=True)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (a.output / 'test.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr, end='')
        raise SystemExit(run.returncode)
    obj = ROOT / 'kernel/desktop_frame_owner.c'
    subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-std=c11', '-O2',
        '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
        '-Wall', '-Wextra', '-Werror', '-mno-red-zone', '-mno-sse',
        '-mno-mmx', '-msoft-float', *inc, '-c', str(obj),
        '-o', str(a.output / 'desktop-frame-owner.obj')], check=True)
    print('DESKTOP FRAME OWNER COFF PASS')


if __name__ == '__main__':
    main()
