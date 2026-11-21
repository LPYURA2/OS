#!/usr/bin/env python3
"""775 kernel admission+port transaction with ASAN/UBSAN and COFF linkage.

This test does not claim an independent graphical ELF process runs on Desktop.
"""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCES=[ROOT/name for name in (
    'kernel/process_runtime.c','kernel/process_scheduler.c',
    'kernel/desktop_process_runtime.c','kernel/desktop_vm_stage.c',
    'kernel/desktop_frame_owner.c','kernel/window_service.c',
    'kernel/user_session_runtime.c','kernel/process_inspector.c',
    'kernel/process_view.c','kernel/process_api.c',
    'kernel/native_launch_stage.c','kernel/native_window_channel.c',
    'kernel/native_desktop_launch.c',
    'kernel/native_desktop_bridge.c','kernel/desktop_draw.c',
    'tests/test_native_desktop_launch.c')]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            '-I',str(ROOT/'include')]
    exe=args.output/'native-desktop-admission-host'
    subprocess.run([cc,*common,'-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',*map(str,SOURCES),
                    '-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (args.output/'test.log').write_text(result.stdout+result.stderr)
    print(result.stdout,end='')
    if result.returncode:
        print(result.stderr,end='')
        raise SystemExit(result.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff',*common,
                    '-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
                    '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float','-c',
                    str(ROOT/'kernel/native_desktop_launch.c'),'-o',
                    str(args.output/'native-desktop-admission.obj')],check=True)
    print('STYLEOS 775 NATIVE ADMISSION COFF PASS')
if __name__=='__main__': main()
