#!/usr/bin/env python3
"""Sanitized guest canonical .tea ZIP parser and disk-header negative tests."""
import argparse
from pathlib import Path
import subprocess
import shutil
import sys
import zipfile
ROOT=Path(__file__).resolve().parents[1]

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(ROOT/'tools'))
    from build_native_tea_768 import build as build768
    from build_native_tea_769 import build as build769
    build768(output)
    build769(output)
    (output/'header.bin').write_bytes((output/'native-tea-769-disk.img').read_bytes()[:512])
    with zipfile.ZipFile(output/'minimal.tea') as z:
        (output/'manifest-exact.json').write_bytes(z.read('manifest.json'))
    cc=shutil.which('clang') or 'clang'
    exe=output/'native-tea-769-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
      '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'include'),
      str(ROOT/'kernel/native_tea_elf.c'),str(ROOT/'kernel/native_tea_package.c'),
      str(ROOT/'tests/test_native_tea_769.c'),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe),str(output/'minimal.tea'),
      str(output/'header.bin'),
      str(output/'manifest-exact.json')],capture_output=True,text=True)
    (output/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='')
    if result.returncode:
        print(result.stderr,end='');raise SystemExit(result.returncode)
    print('TEA769 HOST ASAN UBSAN PASS')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
