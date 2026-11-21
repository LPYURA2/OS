#!/usr/bin/env python3
"""Host + real x86-64 UEFI COFF object tests for desktop app sessions.

Runs without QEMU. The guest-screen/UI integration has separate QEMU checks.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=ROOT / 'build' / 'session-check')
    args = p.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    cc = shutil.which(os.environ.get('CLANG', 'clang'))
    if not cc:
        raise RuntimeError('clang required')
    source = str(ROOT / 'kernel' / 'desktop_sessions.c')
    tests = str(ROOT / 'tests' / 'test_desktop_sessions.c')
    headers = str(ROOT / 'include')
    exe = folder / 'desktop-sessions-host'
    host = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-I', headers, source, tests, '-o', str(exe)]
    subprocess.run(host, check=True)
    result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
    (folder / 'host.log').write_text(result.stdout + result.stderr)
    coff = folder / 'desktop-sessions-x64.obj'
    target = [cc, '--target=x86_64-pc-win32-coff', '-std=c11', '-O2',
              '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
              '-fno-pic', '-fno-pie', '-mno-red-zone', '-mno-sse', '-mno-mmx',
              '-msoft-float', '-Wall', '-Wextra', '-Werror', '-I', headers,
              '-c', source, '-o', str(coff)]
    subprocess.run(target, check=True)
    report = {'host': 'PASS', 'x86_64_coff': 'PASS',
              'scope': 'PID registry and unsaved draft survive logical shell restart; no real .tea loader yet',
              'checks': ['initial and stable PID', 'invalid app/PID refusal',
                         'F10-like shell PID renewal', 'UTF-8 unsaved draft and app PIDs retained',
                         'process termination/relaunch', 'PID exhaustion handling']}
    (folder / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(result.stdout, end='')
    print('DESKTOP SESSIONS HOST/UEFI COFF: PASS')


if __name__ == '__main__':
    run()
