#!/usr/bin/env python3
"""Stage 777 sanitized detached page-root admission tests; NOT hardware proof."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
NAMES=('kernel/desktop_grant_bridge.c','kernel/frame_grant_proof.c',
       'kernel/desktop_vm_stage.c','kernel/desktop_frame_owner.c',
       'kernel/process_runtime.c','kernel/boot_pool.c','kernel/boot_memory.c',
       'tests/test_desktop_grant_bridge.c')
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    o=p.parse_args().output;o.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    inc=['-std=c11','-O1','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=o/'grant-bridge-host'
    subprocess.run([cc,*inc,'-fsanitize=address,undefined','-fno-omit-frame-pointer',
        *[str(ROOT/n) for n in NAMES],'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (o/'test.log').write_text(r.stdout+r.stderr)
    print(r.stdout,end='',flush=True)
    if r.returncode:print(r.stderr,end='',flush=True);raise SystemExit(r.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
        '-fno-stack-protector','-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
        '-I',str(ROOT/'include'),'-c',str(ROOT/'kernel/desktop_grant_bridge.c'),
        '-o',str(o/'grant-bridge.obj')],check=True)
    print('STYLEOS 777 IMAGE-FRAME BRIDGE COFF PASS',flush=True)
if __name__=='__main__':main()
