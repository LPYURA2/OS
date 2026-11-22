#!/usr/bin/env python3
"""Sanitized descriptor state refusal tests and x86 EFI COFF compile."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    src=ROOT/'kernel/descriptor_state_781.c'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=a.output/'dsi781-host'
    subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer',str(src),str(ROOT/'tests/test_descriptor_state_781.c'),
        '-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    print(result.stdout,end='',flush=True)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff',*flags,'-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone',
        '-mno-sse','-mno-mmx','-msoft-float','-c',str(src),
        '-o',str(a.output/'descriptor_state_781.obj')],check=True)
    print('STYLEOS 781 IDT TSS IST COFF PASS',flush=True)
if __name__=='__main__':main()
