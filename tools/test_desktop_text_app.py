#!/usr/bin/env python3
"""Compile Editor/Notes independent document-state module with sanitizers."""
import argparse
from pathlib import Path
import subprocess
import shutil

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    executable=a.output/'text-app-core'
    subprocess.run([shutil.which('clang') or 'clang','-std=c11','-O1','-g',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall',
                    '-Wextra','-Werror','-I',str(ROOT/'include'),
                    str(ROOT/'kernel/desktop_text_app.c'),
                    str(ROOT/'tests/test_desktop_text_app.c'),
                    '-o',str(executable)],check=True)
    r=subprocess.run([str(executable)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    print(r.stdout,end='');print(r.stderr,end='')
    if r.returncode:raise SystemExit(r.returncode)

if __name__=='__main__':
    main()
