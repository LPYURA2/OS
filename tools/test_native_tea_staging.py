#!/usr/bin/env python3
"""Stage 776 sanitized shared disk .tea loader, with real generated fixture.

Host tests prove only the bounded staging semantics, not physical frame
ownership or ordinary Desktop CPL3 execution. QEMU stage 769 separately
exercises this exact C module from a real IDE disk.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def run(out):
    out.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(ROOT/'tools'))
    from build_native_tea_768 import build as build768
    from build_native_tea_769 import build as build769
    fixture=out/'fixture'
    build768(fixture)
    build769(fixture)
    cc=shutil.which('clang') or 'clang'
    common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            '-I',str(ROOT/'include'),'-I',str(fixture)]
    files=[str(ROOT/x) for x in ['kernel/native_tea_elf.c',
         'kernel/native_tea_package.c','kernel/native_tea_staging.c',
         'tests/test_native_tea_staging.c']]
    exe=out/'native-tea-staging-host'
    subprocess.run([cc,*common,'-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',*files,'-o',str(exe)],check=True)
    result=subprocess.run([str(exe),str(fixture/'native-tea-769-disk.img')],
                          text=True,capture_output=True)
    (out/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='',flush=True)
    if result.returncode:
        print(result.stderr,end='')
        raise RuntimeError(f'staging test failed: exit {result.returncode}')
    obj=out/'native-tea-staging.obj'
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
        '-fno-stack-protector','-mno-red-zone','-mno-sse','-mno-mmx',
        '-msoft-float','-I',str(ROOT/'include'),'-c',
        str(ROOT/'kernel/native_tea_staging.c'),'-o',str(obj)],check=True)
    print('STYLEOS 776 SHARED STAGING COFF PASS',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
