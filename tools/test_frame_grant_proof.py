#!/usr/bin/env python3
"""Sanitized policy-only checks and freestanding x86 COFF compilation."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang') or 'clang'
    inc=['-I',str(ROOT/'include')]
    src=[str(ROOT/p) for p in ('kernel/frame_grant_proof.c','kernel/boot_pool.c',
                               'kernel/boot_memory.c','tests/test_frame_grant_proof.c')]
    exe=args.output/'frame-grant-policy-host'
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer',*inc,*src,
        '-o',str(exe)],check=True)
    test=subprocess.run([str(exe)],capture_output=True,text=True)
    (args.output/'test.log').write_text(test.stdout+test.stderr)
    print(test.stdout,end='')
    if test.returncode:
        print(test.stderr,end='');raise SystemExit(test.returncode)
    for name in ['kernel/frame_grant_proof.c','kernel/frame_grant_experiment.c']:
        subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
          '-ffreestanding','-fno-builtin','-fno-stack-protector','-Wall',
          '-Wextra','-Werror','-mno-red-zone','-mno-sse','-mno-mmx',
          '-msoft-float','-DSTYLEOS_TEST_FRAME_GRANTS=1',*inc,'-c',str(ROOT/name),
          '-o',str(args.output/(Path(name).stem+'.obj'))],check=True)
    print('FRAME GRANT POLICY x86 COFF PASS')
if __name__=='__main__':main()
