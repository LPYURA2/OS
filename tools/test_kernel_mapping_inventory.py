#!/usr/bin/env python3
"""Read-only inherited-page accounting: ASAN/UBSAN + UEFI COFF compilation."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    source=ROOT/'kernel/kernel_mapping_inventory.c'
    base=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
          '-I',str(ROOT/'include')]
    exe=args.output/'inventory-test'
    subprocess.run([*base,'-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    str(source),str(ROOT/'tests/test_kernel_mapping_inventory.c'),
                    '-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (args.output/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='',flush=True)
    if result.returncode:print(result.stderr,end='',flush=True);raise SystemExit(result.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
        '-fno-stack-protector','-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
        '-I',str(ROOT/'include'),'-c',str(source),'-o',
        str(args.output/'inventory.obj')],check=True)
    print('STYLEOS 779 INVENTORY COFF PASS',flush=True)
if __name__=='__main__':main()
