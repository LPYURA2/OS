#!/usr/bin/env python3
"""Compile actual freestanding StyleFS/desktop adapter into an ASAN host test."""
import argparse
from pathlib import Path
import subprocess
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    cc = shutil.which('clang')
    if not cc:
        raise RuntimeError('clang unavailable')
    target = folder / 'notes-stylefs-host'
    cmd = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
           '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
           '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
           '-DSTYLEFS_HOST_FORMAT=1', '-I', str(ROOT / 'include'),
           str(ROOT / 'tests/test_notes_file.c'),
           str(ROOT / 'kernel/stylefs.c'),
           str(ROOT / 'kernel/desktop_volume_io.c'),
           str(ROOT / 'kernel/desktop_stylefs.c'),
           '-o', str(target)]
    subprocess.run(cmd, check=True)
    r = subprocess.run([str(target)], capture_output=True, text=True, check=True)
    (folder / 'test.log').write_text(r.stdout + r.stderr)
    print(r.stdout, end='')


if __name__ == '__main__':
    main()
