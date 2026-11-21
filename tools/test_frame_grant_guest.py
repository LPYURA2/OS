#!/usr/bin/env python3
"""Use real QEMU/OVMF and independently derive the linked PE grant RVAs."""
import argparse
import json
import re
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

    if status=='PASS':
        # Independently bind the 779 inventory to the actual runtime image
        # and stack extents, and to the CR3 restored by the hardware test.
        lines=actual.replace(b'\r',b'').splitlines()
        pattern=(rb'STYLEOS KMI 779 INHERITED cr3=(0x[0-9a-f]{16}) '
                 rb'image=(0x[0-9a-f]{16}) stack=(0x[0-9a-f]{16}) '
                 rb'gdt=(0x[0-9a-f]{16}) idt=(0x[0-9a-f]{16}) '
                 rb'tables=(0x[0-9a-f]{16}) rwx=(0x[0-9a-f]{16}) '
                 rb'tr=(0x[0-9a-f]{16}) tss-ist=unverified '
                 rb'devices=unverified wx=unverified active=0')
        found=[re.fullmatch(pattern,line) for line in lines if line.startswith(b'STYLEOS KMI ')]
        if len(found)!=1 or found[0] is None:
            status='FAIL';detail='779 inherited inventory missing, duplicated or malformed'
        else:
            observed=[int(v,16) for v in found[0].groups()]
            def ranges(kind):
                prefix=b'STYLEOS MEMMAP '+kind+b' '
                vals=[]
                for line in lines:
                    if line.startswith(prefix):
                        match=re.fullmatch(re.escape(prefix)+rb'(0x[0-9a-f]{16}) (0x[0-9a-f]{16})',line)
                        if match: vals.append((int(match[1],16),int(match[2],16)))
                return vals
            im=ranges(b'IMAGE');st=ranges(b'STACK')
            restored=re.findall(rb'STYLEOS PAGING X86 RETURN before=(0x[0-9a-f]{16}) '
                                rb'candidate=(0x[0-9a-f]{16}) after=(0x[0-9a-f]{16})',actual)
            def pages(start,end):return ((end+4095)//4096)-(start//4096)
            if (len(im)!=1 or len(st)!=1 or len(restored)!=1 or
                observed[0]!=int(restored[0][2],16) or
                observed[0]!=int(restored[0][0],16) or
                observed[1]!=pages(*im[0]) or observed[2]!=pages(*st[0]) or
                not all(0<n<=64 for n in observed[3:6]) or
                not (observed[1]<=observed[6]<=sum(observed[1:6]))):
                status='FAIL';detail='779 inventory diverges from inherited CR3 or UEFI ranges'
            elif b'STYLEOS KMI 779 INHERITED' not in actual:
                status='FAIL';detail='779 hardware evidence absent'
            else:
                tampered=actual.replace(b' tss-ist=unverified ',b' tss-ist=verified ',1)
                if re.fullmatch(pattern,[line for line in tampered.replace(b'\r',b'').splitlines() if line.startswith(b'STYLEOS KMI ')][0]):
                    status='FAIL';detail='779 proof parser accepted false TSS completion'
    result={'status':status,'details':detail,'branch_scope':'dedicated image-owned frames',
            'candidate_ram_granted':False,'desktop_root_activated':False}
    (a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'{name}: {status} — {detail}')
    if status!='PASS':raise SystemExit('real image-owned grant attestation failed')
    print('STYLEOS 765 QEMU IMAGE FRAME ATTESTATION PASS')

if __name__=='__main__':main()
