#!/usr/bin/env python3
"""Stage 774: sanitized actual desktop strip-compositor pixel bridge fixture."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCES=[ROOT / name for name in (
    'kernel/process_runtime.c', 'kernel/window_service.c',
    'kernel/user_session_runtime.c', 'kernel/native_window_channel.c',
    'kernel/native_desktop_bridge.c', 'kernel/desktop_draw.c',
    'tests/test_native_desktop_bridge.c')]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            '-I',str(ROOT/'include')]
    exe=args.output/'native-desktop-bridge-host'
    subprocess.run([cc,*common,'-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',*map(str,SOURCES),'-o',str(exe)],check=True)
    proc=subprocess.run([str(exe)],text=True,capture_output=True)
    (args.output/'test.log').write_text(proc.stdout+proc.stderr)
    if proc.stdout:print(proc.stdout,end='')
    if proc.returncode:
        print(proc.stderr,end='')
        raise SystemExit(proc.returncode)
    subprocess.run([cc,'--target=x86_64-pc-win32-coff',*common,
                    '-O2','-ffreestanding','-fno-builtin',
                    '-fno-stack-protector','-mno-red-zone','-mno-sse',
                    '-mno-mmx','-msoft-float','-c',
                    str(ROOT/'kernel/native_desktop_bridge.c'),'-o',
                    str(args.output/'native-desktop-bridge.obj')],check=True)
    print('STYLEOS 774 DESKTOP BRIDGE COFF PASS')

if __name__=='__main__':main()
