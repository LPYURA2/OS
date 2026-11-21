#!/usr/bin/env python3
"""Sanitizer and UEFI COFF validation of the 770 native PID→session bridge."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SRC=[ROOT/p for p in ('kernel/process_runtime.c','kernel/window_service.c',
     'kernel/user_session_runtime.c','kernel/native_session_hw_770.c',
     'tests/test_native_session_hw_770.c')]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    cmd=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
         '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'include'),
         *map(str,SRC),'-o',str(out/'session-host')]
    subprocess.run(cmd,check=True)
    test=subprocess.run([str(out/'session-host')],capture_output=True,text=True)
    (out/'test.log').write_text(test.stdout+test.stderr)
    print(test.stdout,end='')
    if test.returncode: raise RuntimeError(test.stderr)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-pic',
        '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
        '-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-c',
        str(ROOT/'kernel/native_session_hw_770.c'),'-o',str(out/'session-efi.obj')],check=True)
    print('NATIVE SESSION 770 COFF PASS')
if __name__=='__main__':main()
