#!/usr/bin/env python3
"""Unit-only check using in-memory synthetic shell suffix; never guest proof."""
import argparse,json,re
from pathlib import Path
from test_desktop_interactive_863 import site_check,verify,EVENT,POLICY,fnv_text,refusals

def main():
    p=argparse.ArgumentParser();p.add_argument('--prior',type=Path,required=True);p.add_argument('--build',type=Path,required=True);a=p.parse_args()
    site=site_check((a.build/'BOOTX64.EFI').read_bytes(),a.build/'kernel.map')
    old_efi=a.prior/'build/x86_64/BOOTX64.EFI';old_map=a.prior/'build/x86_64/kernel.map'
    try:site_check(old_efi.read_bytes(),old_map)
    except ValueError:pass
    else:raise AssertionError('old binary admitted')
    result=json.loads((a.prior/'results.json').read_text());old=dict(result['site'],s863_shell_observe_rva=site['s863_shell_observe_rva']);ref=json.loads((a.prior/'reference.json').read_text());raw=(a.prior/'serial.log').read_bytes();r=result['observations']['rounds'][1]['join_roots']['safe']
    def row(tag,keys,vals):return ('STYLEOS S863 '+tag+' '+' '.join(f'{k}=0x{v:016x}' for k,v in zip(keys,vals))).encode()
    lines=[row('ADMIT',('root','lpc','dispatches'),(r,0x29188086,20)),POLICY,b'STYLEOS S863 STORAGE DENIED',b'STYLEOS DESKTOP DATA UNAVAILABLE',b'STYLEOS S863 CLOCK DENIED',b'STYLEOS DESKTOP KEYBOARD PASS',b'STYLEOS DESKTOP MOUSE PASS',b'STYLEOS DESKTOP READY',row('READY',('root','dispatches','next-pid','next-window','input','x','y','writes'),(r,20,5,5,3,640,600,1024000))]
    for i,(mouse,key,dx) in enumerate([(0,0x40,0),(1,0,20),(0,0x3f,0),(0,0x1e,0),(0,0x44,0),(0,0x58,0)]):
        if i in (0,1,4):lines.append({0:b'STYLEOS DESKTOP INPUT RU',1:b'STYLEOS DESKTOP CURSOR DAMAGE PASS',4:b'STYLEOS DESKTOP SHELL RESTART'}[i])
        lines.append(row('EVENT',EVENT,(r,i+1,1,mouse,key,dx,0,0,0,640 if i==0 else 660,600,4 if i in (2,3) else 0,1,int(i>=4),2 if i>=3 else 0,fnv_text('ф'.encode() if i>=3 else b''),int(i>=3),3 if i>=2 else 0,(2000,340,5000,5000,1024000,0)[i])))
    lines.append(row('EXIT',('root','events','dispatches','used'),(r,6,20,0)))
    sample=raw.rstrip(b'\r\n')+b'\n'+b'\n'.join(lines)+b'\n';ok,obs=verify(sample,33,old,ref);assert ok,obs
    assert not verify(raw,33,old,ref)[0]
    n=refusals(sample,33,old,ref)
    print('S863 validator UNIT ONLY: old PE/UART refused,',n,'synthetic-suffix mutation refusals PASS')
if __name__=='__main__':main()
