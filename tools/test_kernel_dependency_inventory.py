#!/usr/bin/env python3
"""Run read-only 780 PE section and kernel dependency-policy tests."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    clang=shutil.which('clang') or 'clang'
    src=ROOT/'kernel/kernel_dependency_inventory.c'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=a.output/'kdi-host'
    subprocess.run([clang,*flags,'-O1','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer',str(src),str(ROOT/'tests/test_kernel_dependency_inventory.c'),
        '-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stderr or r.stdout)
    print(r.stdout,end='',flush=True)
    subprocess.run([clang,'--target=x86_64-pc-win32-coff',*flags,'-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone',
        '-mno-sse','-mno-mmx','-msoft-float','-c',str(src),
        '-o',str(a.output/'kernel_dependency_inventory.obj')],check=True)
    print('STYLEOS 780 DEPENDENCY COFF PASS',flush=True)
if __name__=='__main__':main()
