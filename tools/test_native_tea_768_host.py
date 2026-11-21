#!/usr/bin/env python3
"""Host sanitizers and freestanding COFF compile of strict ELF64 loader."""
import argparse
from pathlib import Path
import shutil
import subprocess
from build_native_tea_768 import build
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    build(a.output)
    cc=shutil.which('clang') or 'clang'
    target=a.output/'native-tea-elf-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'include'),
        '-I',str(a.output),str(ROOT/'kernel/native_tea_elf.c'),
        str(ROOT/'tests/test_native_tea_elf.c'),'-o',str(target)],check=True)
    r=subprocess.run([str(target)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    print(r.stdout,end='')
    if r.returncode:
        print(r.stderr,end='');raise SystemExit(r.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone',
        '-mno-sse','-mno-mmx','-msoft-float','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'include'),'-c',str(ROOT/'kernel/native_tea_elf.c'),
        '-o',str(a.output/'native-tea-elf.obj')],check=True)
    print('NATIVE TEA ELF COFF PASS')

if __name__=='__main__':main()
