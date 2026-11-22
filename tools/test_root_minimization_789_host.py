#!/usr/bin/env python3
"""Independent host ASAN/UBSAN and freestanding COFF compile."""
import argparse
from pathlib import Path
import subprocess
import shutil
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
clang=shutil.which('clang') or shutil.which('clang-19')
if not clang:raise RuntimeError('clang required')
src=ROOT/'kernel/root_minimization_789.c'; test=ROOT/'tests/test_root_minimization_789.c'
cmd=[clang,'-std=c11','-Wall','-Wextra','-Werror','-O1','-g',
     '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'include'),
     str(src),str(test),'-o',str(a.output/'root789-host')]
subprocess.run(cmd,check=True)
with (a.output/'test.log').open('w') as log:
    subprocess.run([str(a.output/'root789-host')],check=True,stdout=log,stderr=subprocess.STDOUT)
subprocess.run([clang,'-std=c11','--target=x86_64-pc-win32-coff',
    '-ffreestanding','-mno-red-zone','-O2','-I',str(ROOT/'include'),
    '-c',str(src),'-o',str(a.output/'root789.obj')],check=True)
print((a.output/'test.log').read_text().strip());print('STYLEOS 789 HOST SANITIZERS + COFF PASS')
