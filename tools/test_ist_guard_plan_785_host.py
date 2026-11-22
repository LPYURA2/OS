#!/usr/bin/env python3
"""Independent host-only 785 structural guard admission; not hardware evidence."""
import argparse, subprocess, shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    source=ROOT/'kernel/ist_guard_plan_785.c'
    test=ROOT/'tests/test_ist_guard_plan_785.c'
    exe=a.output/'ist-guard-785-host'
    subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(source),str(test),'-o',str(exe)],check=True)
    proc=subprocess.run([str(exe)],text=True,capture_output=True)
    (a.output/'test.log').write_text(proc.stdout+proc.stderr)
    if proc.returncode:raise RuntimeError(proc.stdout+proc.stderr)
    print(proc.stdout,end='')
    coff=a.output/'ist_guard_plan_785.obj'
    subprocess.run([cc,'--target=x86_64-pc-win32-coff',*flags,'-O2',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-c',str(source),'-o',str(coff)],check=True)
    print('STYLEOS 785 STRUCTURAL COFF PASS hardware=not-run activation=0')
if __name__=='__main__': main()
