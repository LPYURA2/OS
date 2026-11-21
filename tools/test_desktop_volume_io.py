#!/usr/bin/env python3
"""Compile the production two-volume block mapping and StyleFS v2 with ASAN/UBSAN."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    clang = shutil.which('clang')
    if not clang:
        raise RuntimeError('clang is required')
    program = folder / 'two-volumes-test'
    subprocess.run([clang, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                    '-DSTYLEFS_HOST_FORMAT=1', '-I', str(ROOT / 'include'),
                    str(ROOT / 'tests/test_desktop_volume_io.c'),
                    str(ROOT / 'kernel/desktop_volume_io.c'),
                    str(ROOT / 'kernel/stylefs.c'), '-o', str(program)], check=True)
    completed = subprocess.run([str(program)], capture_output=True, text=True)
    (folder / 'test.log').write_text(completed.stdout + completed.stderr)
    print(completed.stdout, end='')
    print(completed.stderr, end='')
    if completed.returncode:
        raise RuntimeError(f'volume mapping test failed with exit {completed.returncode}')


if __name__ == '__main__':
    main()
