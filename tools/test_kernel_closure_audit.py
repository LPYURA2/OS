#!/usr/bin/env python3
"""Host structural policy only: success does NOT authorize a hardware CR3 switch."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCES=[ROOT/x for x in (
    'kernel/kernel_closure_audit.c','kernel/desktop_vm_stage.c',
    'kernel/desktop_frame_owner.c','kernel/process_runtime.c',
    'tests/test_kernel_closure_audit.c')]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    base=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=a.output/'closure-host'
    subprocess.run([*base,'-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    *map(str,SOURCES),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='',flush=True)
    if result.returncode:
        print(result.stderr,end='',flush=True)
        raise SystemExit(result.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
      '-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
      '-fno-stack-protector','-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
      '-I',str(ROOT/'include'),'-c',str(ROOT/'kernel/kernel_closure_audit.c'),
      '-o',str(a.output/'kernel_closure_audit.obj')],check=True)
    print('STYLEOS 778 KERNEL CLOSURE COFF PASS',flush=True)
if __name__=='__main__': main()
