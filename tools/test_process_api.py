#!/usr/bin/env python3
"""Host sanitizer and freestanding COFF check for staged process snapshot API."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / p for p in (
    'kernel/process_runtime.c', 'kernel/window_service.c',
    'kernel/process_inspector.c', 'kernel/process_view.c',
    'kernel/process_api.c', 'tests/test_process_api.c')]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT / 'include')]
    executable = args.output / 'process-api-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        *inc, *map(str, SOURCES), '-o', str(executable)], check=True)
    result = subprocess.run([str(executable)], text=True, capture_output=True)
    (args.output / 'test.log').write_text(result.stdout + result.stderr)
    print(result.stdout, end='')
    if result.returncode:
        print(result.stderr, end='')
        raise SystemExit(result.returncode)
    subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-std=c11',
        '-O2', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
        '-Wall', '-Wextra', '-Werror', '-mno-red-zone', '-mno-sse',
        '-mno-mmx', '-msoft-float', *inc, '-c',
        str(ROOT / 'kernel/process_api.c'), '-o',
        str(args.output / 'process-api.obj')], check=True)
    print('PROCESS SNAPSHOT API COFF PASS')

if __name__ == '__main__':
    main()
