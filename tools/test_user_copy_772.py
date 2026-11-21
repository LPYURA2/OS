#!/usr/bin/env python3
"""Sanitized bounded user-copy broker and x86-64 COFF compile, no guest claim."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    cc = shutil.which('clang') or 'clang'
    exe=output/'user-copy-772-host'
    src=ROOT/'kernel/user_copy_772.c'
    inc=['-I',str(ROOT/'include')]
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',*inc,
                    str(src),str(ROOT/'tests/test_user_copy_772.c'),'-o',str(exe)],check=True)
    p=subprocess.run([str(exe)],capture_output=True,text=True)
    (output/'test.log').write_text(p.stdout+p.stderr)
    print(p.stdout,end='')
    if p.returncode: print(p.stderr,end=''); raise SystemExit(p.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
                    '-Wall','-Wextra','-Werror',*inc,'-c',str(src),
                    '-o',str(output/'user_copy_772.obj')],check=True)
    print('USER COPY 772 UEFI COFF PASS')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path)
    run(p.parse_args().output)
