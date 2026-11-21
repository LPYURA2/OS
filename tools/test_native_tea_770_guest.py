#!/usr/bin/env python3
"""Real x86 CPL3 `.tea` from external disk with two per-user session windows.

No actual compositor/window surface is rendered by this diagnostic profile;
all window/session objects are owned by the kernel model, not shell UI IDs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from test import firmware_path
from test_native_tea_769_guest import guest
from test_native_tea_768_guest import check_uart

ROOT=Path(__file__).resolve().parents[1]

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    build=output/'compiled'
    with (output/'build.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'), '--arch','x86_64',
                        '--test-native-tea-session','--output',str(build)],
                       check=True,stdout=log,stderr=subprocess.STDOUT)
    target=build/'x86_64'
    site=json.loads((target/'tea-768-site.json').read_text())
    proof=json.loads((target/'tea-769-site.json').read_text())
    tea=(target/'minimal.tea').read_bytes()
    disk_path=target/'native-tea-769-disk.img'
    disk=disk_path.read_bytes()
    if disk[512:512+len(tea)]!=tea or len(disk)!=128*512 or \
       hashlib.sha256(disk).hexdigest()!=proof['image_sha256']:
        raise AssertionError('disk package metadata differs from verified source')
    if tea in (target/'BOOTX64.EFI').read_bytes() or (target/'app.elf').read_bytes() in (target/'BOOTX64.EFI').read_bytes():
        raise AssertionError('embedded .tea or ELF payload (not a guest disk execution proof)')
    qemu=shutil.which(os.environ.get('QEMU_X86','qemu-system-x86_64'))
    code,var=firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('real QEMU+OVMF required, no synthetic PASS')
    raw=guest(qemu,code,var,target/'styleos.img',disk_path,output/'positive',75)
    lines=raw.splitlines()
    required=[
        b'STYLEOS TEA770 SESSION REGISTERED real_pids=2 separated_roots=2 windows=2',
        b'STYLEOS TEA770 USER A FAULT WINDOW REAP PASS',
        b'STYLEOS TEA770 USER B EXIT WINDOW REAP PASS',
        b'STYLEOS TEA770 PID ROOT SESSION WINDOW PASS',
        b'STYLEOS TEA769 DISK CPL3 EXIT PASS',
        b'STYLEOS USER PASS', b'STYLEOS KERNEL HALT',
    ]
    for m in required:
        if lines.count(m)!=1: raise AssertionError(f'missing/duplicate real guest marker {m!r}')
    if not (lines.index(required[0]) < lines.index(required[1]) <
            lines.index(required[2]) < lines.index(required[3]) < lines.index(required[4])):
        raise AssertionError('lifecycle markers appear out of observed trap order')
    compatible=raw.replace(b'STYLEOS TEA769',b'STYLEOS TEA768').replace(
         b'STYLEOS TEA768 DISK CPL3 EXIT PASS', b'STYLEOS TEA768 CPL3 EXIT PASS')
    old_proof=check_uart(compatible,site)
    if hashlib.sha256(disk_path.read_bytes()).hexdigest()!=proof['image_sha256']:
        raise AssertionError('positive guest modified the immutable .tea disk')
    tamper=bytearray(disk);tamper[512+len(tea)//2]^=1
    bad=output/'tampered.img';bad.write_bytes(tamper)
    negative=guest(qemu,code,var,target/'styleos.img',bad,output/'negative',40)
    if not any(line.startswith(b'STYLEOS USER ERROR:') for line in negative.splitlines()) or \
       b'STYLEOS USER FRAME ' in negative or b'STYLEOS TEA770 SESSION REGISTERED' in negative:
        raise AssertionError('tampered package did not fail before process/session admission')
    if bad.read_bytes()!=bytes(tamper):
        raise AssertionError('QEMU modified the rejected disk input')
    result={
        'result':'PASS','stage':770,'scope':'dedicated diagnostic CPL3 guest only',
        'hardware_cpl3':old_proof,'per_user_session_windows':2,
        'isolated_root_bindings':2,'compositor_restart_preserves_windows':True,
        'fault_and_exit_release_windows':True,'tamper_rejected_before_admission':True,
        'disk_sha256':proof['image_sha256'],
        'source_guest_uart_sha256':hashlib.sha256(raw).hexdigest(),
        'limitations':['No actual Desktop launch, browser, UI window surface or authenticated login',
                       'No DFO frame lease transaction or desktop MMU root ownership',
                       'No filesystem ACL, account persistence or general .tea loader',
                       'Dedicated test accounts are constants, not real authenticated users']
    }
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('TEA770 REAL DISK CPL3 PID ROOT USER SESSION WINDOW LIFECYCLE PASS',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
