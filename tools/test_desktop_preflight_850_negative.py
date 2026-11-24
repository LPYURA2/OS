#!/usr/bin/env python3
"""Synthetic parser unit checks or mutation of an actual valid guest capture."""
import argparse
from pathlib import Path
from desktop_preflight_850_verify import POLICY,verify

def row(tag,**fields):return ('STYLEOS '+tag+' '+' '.join(f'{k.replace("_","-")}=0x{v:016x}' for k,v in fields.items())).encode()
def fixture():
    # Parser fixture only: never use this as hardware evidence.
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',b'STYLEOS MEMMAP IMAGE 0x1000000 0x1400000',b'STYLEOS MEMMAP STACK 0x13e0000 0x13f0000',b'STYLEOS MEMMAP HANDOFF 0x13f0000 0x13f0040',b'STYLEOS MEMMAP SOURCE 0x1500000 0x1504000',b'STYLEOS MEMMAP RAM 0x100000 0x8000000',b'STYLEOS MEMMAP PASS',row('S842 READY',old=0x7801000,new=0x1040000,low_table=0x1020000,low_used=8,lapic=0xfee00000,ioapic=0xfec00000,uc_index=3,pat=0x0007040600070406,ist=0x400000003000),row('S850 GOP',base=0x80000000,bytes=0x3e8000,width=1280,height=800,stride=1280,format=1),row('S850 BAR',dev=1,fn=0,command=7,raw=0x80000008,mask=0xff000008,size=0x1000000),row('S850 ROOT',inherited=0x7801000,live=0x7801000,detached=0x1060000,source=0x1020000,used=12,pages=1000,pat_index=3,cache_bits=24,pat=0x0007040600070406),row('S850 DESCRIPTORS',gdt=0x1044000,idt=0x1045000,tss=0x1046000,rsp0=0x1047000,ist1=0x1048000,ist2=0x1049000,ist3=0x104a000),POLICY])+b'\n'
def mutate(raw,tag,key,value):
    lines=raw.splitlines();prefix=('STYLEOS '+tag+' ').encode()
    for i,line in enumerate(lines):
        if line.startswith(prefix):
            import re
            pat=rb'(?<= )'+key.encode()+rb'=0x[0-9a-f]{16}'
            lines[i],n=re.subn(pat,key.encode()+f'=0x{value:016x}'.encode(),line)
            if n!=1:raise AssertionError(key)
            return b'\n'.join(lines)+b'\n'
    raise AssertionError(tag)
def main():
    p=argparse.ArgumentParser();p.add_argument('--serial',type=Path);args=p.parse_args()
    raw=args.serial.read_bytes() if args.serial else fixture();ok,data=verify(raw,33);assert ok,data
    g,b,r,d=(data[k] for k in ('gop','bar','root','descriptors'))
    cases=[(raw,0),(raw,None),(raw.replace(POLICY,b''),33),(raw+POLICY+b'\n',33),(raw.replace(b'bar-restored=yes',b'bar-restored=no'),33),(raw.replace(b'desktop-root-active=no',b'desktop-root-active=yes'),33),(raw.replace(b'framebuffer-write=no',b'framebuffer-write=yes'),33), (raw+b'STYLEOS S807 PASS\n',33)]
    for tag,key,val in [('S850 GOP','base',g['base']+1),('S850 GOP','stride',g['width']-1),('S850 GOP','format',2),('S850 BAR','mask',0),('S850 BAR','command',0),('S850 BAR','raw',b['raw']^1),('S850 ROOT','live',r['detached']),('S850 ROOT','detached',r['source']),('S850 ROOT','used',33),('S850 ROOT','pages',r['pages']+1),('S850 ROOT','cache-bits',r['cache-bits']^8),('S850 ROOT','pat-index',8),('S850 DESCRIPTORS','ist2',d['ist1']),('S850 DESCRIPTORS','gdt',r['detached']),('S850 DESCRIPTORS','tss',0)]:cases.append((mutate(raw,tag,key,val),33))
    cases.append((raw+b'STYLEOS MEMMAP RAM '+f'0x{g["base"]:016x} 0x{g["base"]+4096:016x}\n'.encode(),33))
    for i,(bad,rc) in enumerate(cases):assert not verify(bad,rc)[0],i
    print(f'S850 parser: {len(cases)}/{len(cases)} negative PASS ({"actual capture mutations" if args.serial else "synthetic only"})')
if __name__=='__main__':main()
