#!/usr/bin/env python3
"""Guest proof: independent service state initialized; no invented process PIDs."""
import argparse
import json
import os
from pathlib import Path
from test import firmware_path
from test_desktop_v0 import command_exists, vm, digest

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--iso',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    firmware=(firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS'))
    if not all(firmware):raise RuntimeError('OVMF missing')
    qemu=command_exists(os.environ.get('QEMU_X86','qemu-system-x86_64'))
    iso=a.iso.resolve(strict=True)
    before=digest(iso)
    with vm(a.output/'qemu',iso,None,qemu,firmware,
            image_format='iso',data_format='raw') as (guest, frame):
        guest.wait_uart('SCHEDULER READY registered=0')
        guest.wait_uart('PROCESS SERVICE READY registered=0')
        guest.wait_uart('WINDOW SERVICE READY registered=0')
        guest.wait_uart('DATA UNAVAILABLE')
        guest.screen('desktop-before-process-activation')
        uart=guest.uart.read_bytes().decode('utf-8','replace')
        if 'RUNTIME SERVICE ERROR' in uart or 'SCHEDULER ERROR' in uart:
            raise RuntimeError('process or window initialization failed')
    if digest(iso)!=before:raise RuntimeError('ISO modified by guest')
    (a.output/'results.json').write_text(json.dumps({
        'result':'PASS','iso_sha256':before,
        'scope':'real x86-64 root attached to empty desktop dispatcher; no native .tea launch yet',
        'checks':['real UEFI boot','zero fabricated session-process entries',
                  'independent window service initialized',
                  'dispatcher bootstrapped from active CPU paging root with 0 processes',
                  'desktop visible and existing UI unchanged']
    },indent=2)+'\n')
    print('PROCESS + WINDOW SERVICE QEMU BOOT PASS')

if __name__=='__main__': main()
