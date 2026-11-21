#!/usr/bin/env python3
"""Run real production StyleFS v2 staged-copy host regression with ASAN/UBSAN."""
import argparse
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    clang = shutil.which('clang')
    if not clang:
        raise RuntimeError('clang required')
    target = folder / 'stylefs-background-copy-host'
    subprocess.run([clang, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-DSTYLEFS_HOST_FORMAT=1', '-I', str(ROOT / 'include'),
                    str(ROOT / 'tests/test_stylefs_background_copy.c'),
                    str(ROOT / 'kernel/stylefs.c'), '-o', str(target)], check=True)
    completed = subprocess.run([str(target)], text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    (folder / 'test.log').write_text(completed.stdout, encoding='utf-8')
    print(completed.stdout, end='')
    if completed.returncode:
        raise SystemExit(completed.returncode)

if __name__ == '__main__':
    main()
