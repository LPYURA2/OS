#!/usr/bin/env python3
"""QEMU hardware-trap regression for the process-registry scheduler bridge."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from test import Checks, firmware_path

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--arch', choices=['all','x86_64','aarch64'], default='all')
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    checks = Checks(args.output / 'checks')
    arches = ['x86_64','aarch64'] if args.arch == 'all' else [args.arch]
    for arch in arches:
        dest = args.output / ('build-' + arch)
        subprocess.run([sys.executable, str(ROOT / 'tools/build.py'),
            '--arch', arch, '--test-user-processes', '--output', str(dest)],
            check=True, stdout=subprocess.DEVNULL)
        cmd = os.environ.get('QEMU_X86' if arch == 'x86_64' else 'QEMU_AARCH64',
                             'qemu-system-x86_64' if arch == 'x86_64' else 'qemu-system-aarch64')
        binary = shutil.which(cmd)
        if not binary:
            raise RuntimeError('Real QEMU required: ' + cmd)
        firmware = None
        if arch == 'x86_64':
            code = firmware_path('OVMF_CODE', 'OVMF_CODE')
            var = firmware_path('OVMF_VARS', 'OVMF_VARS')
            if not code or not var:
                raise RuntimeError('OVMF_CODE/OVMF_VARS required')
            firmware = (code, var)
        for el2 in ((False, True) if arch == 'aarch64' else (False,)):
            name = 'scheduler-' + arch + ('-el2' if el2 else '')
            job = (name, arch, binary, dest, firmware, 'user-processes', el2, 128, 90)
            actual, status, detail, log = checks.run_qemu(job)
            (args.output / (name + '.log')).write_text(log)
            results.append({'name': actual, 'status': status, 'detail': detail})
            print(f'{actual}: {status} — {detail}', flush=True)
    (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    if any(r['status'] != 'PASS' for r in results):
        raise SystemExit('hardware guest scheduler trace failed')
    print('PROCESS SCHEDULER HARDWARE QEMU PASS')

if __name__ == '__main__':
    main()
