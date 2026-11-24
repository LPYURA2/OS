import argparse,re
from pathlib import Path
from user_preempt_848_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('READY','safe'),('READY','a-loop-end'),('READY','lapic'),('TICK','n'),('TICK','id'),('TICK','root'),('TICK','rip'),('TICK','rsp'),('TICK','flags'),('TICK','frame'),('TICK','cs'),('TICK','ss'),('TICK','isr'),('TICK','eoi'),('TICK','rax'),('TICK','r15'),('BACK','a'),('BACK','b'),('BACK','safe'),('BACK','state'),('BACK','stack-same'),('END','vector'),('END','cr2'),('END','state'),('TOTAL','ticks'),('TOTAL','restored')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S848 '+tag+' ').encode()))
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^(0x10000 if key=="isr" else 0x100000000000):016x}'.encode(),bad[i],count=1)
 assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
for tag in (b'READY',b'TICK',b'BACK',b'END',b'TOTAL',b'POLICY'):
 cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S848 '+tag+b' '))+b'\n')
cases.append(raw.replace(b'cpl3-timer=yes',b'cpl3-timer=no'))
for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
assert not verify(raw,0)[0]
print(f'S848 genuine-capture host refusals: {len(cases)+1} PASS')
