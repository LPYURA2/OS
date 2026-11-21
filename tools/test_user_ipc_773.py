#!/usr/bin/env python3
"""773 host ASAN/UBSAN and x86 COFF tests. Not a hardware-test substitute."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    clang=shutil.which('clang') or 'clang'
    kernel=['kernel/process_runtime.c','kernel/process_scheduler.c',
      'kernel/window_service.c','kernel/user_session_runtime.c',
      'kernel/native_session_hw_770.c','kernel/native_window_channel.c',
      'kernel/user_ipc_773.c','kernel/user_copy_772.c']
    src=[str(ROOT/x) for x in kernel]
    args=[clang,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
          '-fsanitize=address,undefined','-fno-omit-frame-pointer',
          '-I',str(ROOT/'include'),*src,str(ROOT/'tests/test_user_ipc_773.c'),
          '-o',str(output/'test_user_ipc_773')]
    subprocess.run(args,check=True)
    done=subprocess.run([str(output/'test_user_ipc_773')],capture_output=True,text=True)
    (output/'host.log').write_text(done.stdout+done.stderr)
    print(done.stdout,end='',flush=True)
    if done.returncode:
        print(done.stderr,end='');raise SystemExit(done.returncode)
    for name in ('kernel/user_ipc_773.c','kernel/user_copy_772.c'):
        subprocess.run([clang,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
         '-ffreestanding','-fno-builtin','-fno-stack-protector',
         '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
         '-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-c',str(ROOT/name),
         '-o',str(output/(Path(name).stem+'.obj'))],check=True)
    print('IPC 773 x86 COFF COMPILE PASS')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    run(ap.parse_args().output)
