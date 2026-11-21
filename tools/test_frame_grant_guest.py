#!/usr/bin/env python3
"""Use real QEMU/OVMF and independently derive the linked PE grant RVAs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from test import Checks,firmware_path,serial_frame_grants_ok,verified_paging_site
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    built=a.output/'build'
    subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
      '--test-frame-grants','--output',str(built)],check=True,stdout=subprocess.DEVNULL)
    emulator=shutil.which('qemu-system-x86_64')
    fw=(firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS'))
    if not emulator or not all(fw):
        raise RuntimeError('Real QEMU/OVMF required; no artificial PASS')
    check=Checks(a.output/'hardware')
    name,status,detail,uart=check.run_qemu(('frame-grants-x86','x86_64',emulator,built,fw,
                                            'paging-grants',False,128,90))
    (a.output/'serial.log').write_text(uart)
    site=verified_paging_site(built,'x86_64',fault=False,roundtrip=True)
    actual=uart.encode() if isinstance(uart,str) else uart
    if status=='PASS':
        # A PASS must be tamper-sensitive: altered physical translation and
        # removed proof must fail the independent verifier as well.
        data=actual.replace(b'STYLEOS GRANTS PASS pages=',b'STYLEOS GRANTS PASS pagesX=',1)
        if serial_frame_grants_ok(data,'x86_64',site,built/'x86_64'/'kernel.map'):
            status='FAIL';detail='proof parser accepted tampered UART'
        if status=='PASS':
            altered=actual.replace(b'STYLEOS DGB PASS pages=79',
                                   b'STYLEOS DGB PASX pages=79',1)
            if serial_frame_grants_ok(altered,'x86_64',site,built/'x86_64'/'kernel.map'):
                status='FAIL';detail='bridge parser accepted tampered UART'
    if status=='PASS':
        marker=b'STYLEOS KCA 778 REFUSED missing-kernel-closure=1 active=0 no-cr3-switch=1'
        if actual.count(marker)!=1:
            status='FAIL';detail='778 closure refusal missing or duplicated in real UART'
        elif marker in actual.replace(marker,b'BROKEN MARKER',1):
            status='FAIL';detail='778 marker tamper check invalid'

    result={'status':status,'details':detail,'branch_scope':'dedicated image-owned frames',
            'candidate_ram_granted':False,'desktop_root_activated':False}
    (a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'{name}: {status} — {detail}')
    if status!='PASS':raise SystemExit('real image-owned grant attestation failed')
    print('STYLEOS 765 QEMU IMAGE FRAME ATTESTATION PASS')

if __name__=='__main__':main()
