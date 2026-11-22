#!/usr/bin/env python3
"""Sanitizer and freestanding ABI proof for bounded owned low-half mapper."""
import argparse
from pathlib import Path
import subprocess
import shutil
ROOT=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);ns=a.parse_args()
ns.output.mkdir(parents=True,exist_ok=True)
cc=shutil.which('clang')
if not cc:raise RuntimeError('clang required')
include=['-I',str(ROOT/'include')]
subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-O1','-g',
 '-D_GNU_SOURCE','-fsanitize=address,undefined','-fno-omit-frame-pointer',*include,
 str(ROOT/'kernel/owned_low_790.c'),str(ROOT/'kernel/paging.c'),
 str(ROOT/'tests/test_owned_low_790.c'),'-o',str(ns.output/'owned790-host')],check=True)
with (ns.output/'test.log').open('w') as f:
 subprocess.run([str(ns.output/'owned790-host')],check=True,stdout=f,stderr=subprocess.STDOUT)
for src in ['kernel/owned_low_790.c']:
 subprocess.run([cc,'--target=x86_64-pc-win32-coff','-O2','-ffreestanding',
 '-mno-red-zone','-mno-sse','-Wall','-Wextra','-Werror',*include,
 '-c',str(ROOT/src),'-o',str(ns.output/'owned790.obj')],check=True)
print((ns.output/'test.log').read_text().strip())
print('STYLEOS 790 HOST ASAN UBSAN COFF PASS')
