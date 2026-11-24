import argparse,re
from pathlib import Path
from task_stacks_845_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('LEASE','gen-a'),('LEASE','base'),('STACK','n'),('STACK','rsp'),('STACK','a'),('STACK','b'),('RELEASE','root'),('RELEASE','rsp'),('RELEASE','frame'),('RELEASE','flags'),('RELEASE','zero'),('RELEASE','used'),('RELEASE','reuse-gen')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S845 '+tag+' ').encode()))
 bad=lines[:];mask=0x200 if key=='flags' else 0x100000000000
 bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^mask:016x}'.encode(),bad[i],count=1)
 assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
for tag in (b'LEASE',b'STACK',b'RELEASE',b'POLICY'):
 cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S845 '+tag+b' '))+b'\n')
cases.append(raw.replace(b'terminal=yes contexts-cleared=yes',b'terminal=no contexts-cleared=no'))
for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
assert not verify(raw,0)[0]
print(f'S845 genuine-capture host refusals: {len(cases)+1} PASS')
