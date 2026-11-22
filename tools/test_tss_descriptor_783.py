#!/usr/bin/env python3
"""ASAN/UBSAN and real x86-64 EFI COFF compilation for stage 783."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=a.output/'tgd783-host'
    subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer',str(ROOT/'kernel/tss_descriptor_783.c'),
        str(ROOT/'kernel/tss_ist_stage_782.c'),
        str(ROOT/'kernel/frame_grant_proof.c'),str(ROOT/'kernel/boot_pool.c'),
        str(ROOT/'tests/test_tss_descriptor_783.c'),'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    print(r.stdout,end='',flush=True)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff',*flags,'-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone',
        '-mno-sse','-mno-mmx','-msoft-float','-c',
        str(ROOT/'kernel/tss_descriptor_783.c'),
        '-o',str(a.output/'tss_descriptor_783.obj')],check=True)
    print('STYLEOS 783 TSS DESCRIPTOR COFF PASS',flush=True)
if __name__=='__main__':main()
