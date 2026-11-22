#!/usr/bin/env python3
"""Mandatory CI join of two *fresh* independent real QEMU hardware runs."""
import argparse,hashlib,json,sys
from pathlib import Path
from cache_802_verify import reconcile

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--cache-800',type=Path,required=True)
    p.add_argument('--cache-801',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    files=[]
    for expected,d in ((800,a.cache_800),(801,a.cache_801)):
        r=json.loads((d/'results.json').read_text())
        raw=(d/'serial.log').read_bytes()
        if (r.get('stage')!=expected or r.get('status')!='PASS' or not r.get('qemu_hardware_observed')
          or r.get('qemu_exit_code')!=33 or r.get('uart_sha256')!=hashlib.sha256(raw).hexdigest()
          or r.get('production_cr3_authorized') is not False or not r.get('efi_sha256')):
            raise SystemExit(f'S{expected}: invalid or unbound QEMU evidence')
        files.append((raw,r))
    valid,record=reconcile(files[0][0],files[1][0],33,33)
    if not valid:raise SystemExit('Stage 802 correlation FAIL: '+str(record))
    out=a.output;out.mkdir(parents=True,exist_ok=True)
    record['S800_uart_sha256']=files[0][1]['uart_sha256']
    record['S801_uart_sha256']=files[1][1]['uart_sha256']
    record['S800_efi_sha256']=files[0][1]['efi_sha256']
    record['S801_efi_sha256']=files[1][1]['efi_sha256']
    (out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
    (out/'evidence.log').write_text('S802 PASS: independent QEMU S800/S801 evidence correlated; three UC samples; global alias/production CR3 NOT verified\n')
    print('STYLEOS 802 cross-QEMU cache reconciliation PASS, 3 samples, production CR3 DENIED')

if __name__=='__main__':main()
