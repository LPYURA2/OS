#!/usr/bin/env python3
"""Sanitized process-owned window service and approved installer modal tests."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    sources=[str(ROOT/'kernel/process_runtime.c'),str(ROOT/'kernel/window_service.c'),
             str(ROOT/'tests/test_window_service.c')]
    executable=a.output/'window-service-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I',str(ROOT/'include'),*sources,'-o',str(executable)],check=True)
    result=subprocess.run([str(executable)],text=True,capture_output=True)
    (a.output/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='')
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
                    '-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),
                    '-c',str(ROOT/'kernel/window_service.c'),
                    '-o',str(a.output/'window-service.obj')],check=True)
    print('WINDOW SERVICE COFF PASS')
if __name__=='__main__':main()
