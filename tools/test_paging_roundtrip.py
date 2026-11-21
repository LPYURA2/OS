#!/usr/bin/env python3
"""Real QEMU-OVMF, independently verified CPL0 CR3 candidate→inherited return."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from test import Checks, firmware_path
ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    build = args.output / 'build'
    subprocess.run([sys.executable, str(ROOT / 'tools/build.py'), '--arch', 'x86_64',
                    '--test-paging-roundtrip', '--output', str(build)],
                   check=True, stdout=subprocess.DEVNULL)
    qemu = shutil.which('qemu-system-x86_64')
    code, var = firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('real QEMU-OVMF required; no synthetic PASS')
    checks = Checks(args.output / 'hardware')
    name, status, detail, log = checks.run_qemu(
        ('paging-roundtrip-x86', 'x86_64', qemu, build, (code,var),
         'paging-roundtrip', False, 128, 90))
    (args.output / 'serial.log').write_text(log)
    (args.output / 'results.json').write_text(json.dumps({
        'profile': 'dedicated CPL0 paging', 'result': status,
        'detail': detail, 'not_desktop_dvm_roots': True,
    }, indent=2) + '\n')
    print(f'{name}: {status} — {detail}', flush=True)
    if status != 'PASS':
        raise SystemExit('QEMU CR3 roundtrip proof failed')
    print('STYLEOS 763 HARDWARE CR3 ROUNDTRIP PASS', flush=True)

if __name__ == '__main__':
    main()
