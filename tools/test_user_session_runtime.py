#!/usr/bin/env python3
"""Sanitized user-session boundary test and freestanding x86-64 COFF compile."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    inc=['-I',str(ROOT/'include')]
    sources=[ROOT/'kernel/process_runtime.c',ROOT/'kernel/window_service.c',
             ROOT/'kernel/user_session_runtime.c',ROOT/'tests/test_user_session_runtime.c']
    exe=args.output/'user-session-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    *inc,*map(str,sources),'-o',str(exe)],check=True)
    run=subprocess.run([str(exe)],text=True,capture_output=True)
    (args.output/'test.log').write_text(run.stdout+run.stderr)
    print(run.stdout,end='')
    if run.returncode:
        print(run.stderr,end='')
        raise SystemExit(run.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
                    '-Wall','-Wextra','-Werror',*inc,'-c',
                    str(ROOT/'kernel/user_session_runtime.c'),
                    '-o',str(args.output/'user-session.obj')],check=True)
    print('USER SESSION COFF PASS')

if __name__=='__main__':main()
