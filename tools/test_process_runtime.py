#!/usr/bin/env python3
"""Sanitized host test of production process lifecycle & an independent COFF object."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    args=[str(ROOT/'kernel/process_runtime.c'),str(ROOT/'tests/test_process_runtime.c')]
    exe=a.output/'process-runtime-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I',str(ROOT/'include'),*args,'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],text=True,capture_output=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    print(r.stdout,end='')
    if r.returncode:
        print(r.stderr)
        raise SystemExit(r.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
                    '-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),
                    '-c',str(ROOT/'kernel/process_runtime.c'),
                    '-o',str(a.output/'process-runtime.obj')],check=True)
    print('PROCESS REGISTRY COFF PASS')
if __name__=='__main__': main()
