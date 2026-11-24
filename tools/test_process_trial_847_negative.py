import argparse,re
from pathlib import Path
from process_trial_847_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('READY','safe'),('READY','cs'),('READY','tr'),('READY','a-fault'),('TRAP','n'),('TRAP','root'),('TRAP','vector'),('TRAP','error'),('TRAP','rip'),('TRAP','rsp'),('TRAP','frame'),('TRAP','result'),('TRAP','action'),('BACK','root'),('BACK','state'),('BACK','value'),('BACK','foreign'),('BACK','stack-same')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S847 '+tag+' ').encode()))
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^0x100000000000:016x}'.encode(),bad[i],count=1)
 assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
for tag in (b'READY',b'TRAP',b'BACK',b'POLICY'):
 cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S847 '+tag+b' '))+b'\n')
cases.append(raw.replace(b'user-fault-contained=yes',b'user-fault-contained=no'))
i=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S847 TRAP ') and b'vector=0x000000000000000e' in x)
for key in ('cr2','error','frame'):
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^1:016x}'.encode(),bad[i],count=1);cases.append(b'\n'.join(bad)+b'\n')
for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
assert not verify(raw,0)[0]
print(f'S847 genuine-capture host refusals: {len(cases)+1} PASS')
