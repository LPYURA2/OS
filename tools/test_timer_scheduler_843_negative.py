import argparse,re
from pathlib import Path
from timer_scheduler_843_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();i=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 TICK '))
cases=[]
for field in (b'n',b'from',b'to',b'root',b'frame',b'rip',b'rsp',b'a',b'b',b'gprs',b'eoi',b'r15',b'rax'):
    bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+field+rb'=0x([0-9a-f]{16})',lambda m:field+b'=0x'+f'{int(m[1],16)^0x100000000000:016x}'.encode(),bad[i],count=1)
    assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
cases.extend([b'\n'.join(lines[:i]+lines[i+1:])+b'\n',b'\n'.join(lines[:i]+[lines[i]]+lines[i:])+b'\n',raw.replace(b'cpl3=no production-cr3=deny',b'cpl3=yes production-cr3=allow')])
for bad in cases:assert not verify(bad,33)[0]
assert not verify(raw,0)[0]
print(f'S843 genuine-capture host refusals: {len(cases)+1} PASS')
