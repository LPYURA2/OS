#!/usr/bin/env python3
"""Host ASAN/UBSAN + x86_64 UEFI cross-compile of process view model."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'; inc=['-I',str(ROOT/'include')]
    files=[str(ROOT/x) for x in (
        'kernel/process_runtime.c','kernel/window_service.c',
        'kernel/process_inspector.c','kernel/process_view.c',
        'tests/test_process_view.c')]
    exe=a.output/'process-view-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
        *inc,*files,'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    print(r.stdout,end='')
    if r.returncode: print(r.stderr,end='');raise SystemExit(r.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector','-Wall',
        '-Wextra','-Werror','-mno-red-zone','-mno-sse','-mno-mmx',
        '-msoft-float',*inc,'-c',str(ROOT/'kernel/process_view.c'),
        '-o',str(a.output/'process-view.obj')],check=True)
    print('TASK MANAGER VIEW COFF PASS')
if __name__=='__main__': main()
