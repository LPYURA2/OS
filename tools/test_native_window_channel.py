#!/usr/bin/env python3
"""ASAN/UBSAN kernel-only window IPC + freestanding x86-64 COFF check."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / p for p in (
    'kernel/process_runtime.c', 'kernel/window_service.c',
    'kernel/user_session_runtime.c', 'kernel/native_window_channel.c',
    'tests/test_native_window_channel.c')]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    inc = ['-I', str(ROOT/'include')]
    exe = args.output/'native-window-channel-host'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    *inc, *map(str, SOURCES), '-o', str(exe)], check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    (args.output/'test.log').write_text(run.stdout+run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr, end='')
        raise SystemExit(run.returncode)
    subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-std=c11', '-O2',
                    '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
                    '-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float',
                    '-Wall', '-Wextra', '-Werror', *inc, '-c',
                    str(ROOT/'kernel/native_window_channel.c'), '-o',
                    str(args.output/'native-window-channel.obj')], check=True)
    print('NATIVE WINDOW CHANNEL UEFI COFF PASS')


if __name__ == '__main__':
    main()
