#!/usr/bin/env python3
"""Require an actual accepted raw guest capture before checking mutations."""
import argparse,re
from pathlib import Path
from fx_context_849_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);raw=p.parse_args().input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('MODE','ecx'),('MODE','edx'),('MODE','cr0'),('MODE','cr4'),('READY','mask'),('READY','safe'),('CAPTURE','id'),('CAPTURE','root'),('CAPTURE','n'),('CAPTURE','xmm14'),('CAPTURE','xmm15'),('CAPTURE','mxcsr'),('CAPTURE','st0'),('CAPTURE','exp'),('CAPTURE','kernel-restored'),('CAPTURE','terminal')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S849 '+tag+' ').encode()))
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^(1<<27 if key=="ecx" else 1<<24 if key=="edx" else 2 if key=="cr0" else 1<<18 if key=="cr4" else 1<<63):016x}'.encode(),bad[i],count=1)
 assert bad[i]!=lines[i];cases.append(bad)
for tag in (b'MODE',b'READY',b'CAPTURE',b'CLEAN',b'POLICY'):
 cases.append([x for x in lines if not x.startswith(b'STYLEOS S849 '+tag+b' ')])
i=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S849 CAPTURE '));bad=lines[:];bad[i],bad[i+1]=bad[i+1],bad[i];cases.append(bad)
for i,case in enumerate(cases):assert not verify(b'\n'.join(case)+b'\n',33)[0],i
assert not verify(raw,0)[0]
print(f'S849 genuine-capture refusals: {len(cases)+1} PASS')
