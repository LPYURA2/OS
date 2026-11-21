#!/usr/bin/env python3
"""Host ASAN/UBSAN plus freestanding x86-64 COFF build of dormant VM tables."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT / 'include')]
    obj = ROOT / 'kernel/desktop_vm_stage.c'
    test = ROOT / 'tests/test_desktop_vm_stage.c'
    exe = args.output / 'desktop-vm-stage-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    *inc, str(obj), str(test), '-o', str(exe)], check=True)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (args.output / 'test.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr, end='')
        raise SystemExit(run.returncode)
    subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-std=c11', '-O2',
                    '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
                    '-Wall', '-Wextra', '-Werror', '-mno-red-zone', '-mno-sse',
                    '-mno-mmx', '-msoft-float', *inc, '-c', str(obj),
                    '-o', str(args.output / 'desktop-vm-stage.obj')], check=True)
    print('DESKTOP VM STAGE COFF PASS')

if __name__ == '__main__':
    main()
