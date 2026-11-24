import argparse,re
from pathlib import Path
from physical_arena_844_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('RESERVE','pages'),('ADMIT','type'),('ADMIT','attr'),('ADMIT','base'),('LIVE','root'),('LIVE','gen-reuse'),('LIVE','used'),('LIVE','zero'),('LIVE','refusals')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S844 '+tag+' ').encode()))
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^0x1000:016x}'.encode(),bad[i],count=1)
 if key=='attr':bad[i]=bad[i].replace(b'attr=0x000000000000100f',b'attr=0x8000000000000000');bad[i]=re.sub(rb'attr=0x[0-9a-f]{16}',b'attr=0x8000000000000000',bad[i])
 assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
for tag in (b'RESERVE',b'ADMIT',b'LIVE',b'POLICY'):
 cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S844 '+tag+b' '))+b'\n')
cases.append(raw.replace(b'candidate-grants=no reclaim=no',b'candidate-grants=yes reclaim=no'))
for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
assert not verify(raw,0)[0]
print(f'S844 genuine-capture host refusals: {len(cases)+1} PASS')
