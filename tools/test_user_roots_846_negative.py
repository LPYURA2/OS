import argparse,re
from pathlib import Path
from user_roots_846_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();cases=[]
for tag,key in [('READY','safe'),('READY','a'),('READY','pa-b'),('READY','gen-a'),('READY','ist3'),('READY','used-b'),('SWITCH','live'),('SWITCH','after'),('SWITCH','data'),('SWITCH','foreign-data'),('SWITCH','write-stack'),('SWITCH','g2'),('CLEAN','used'),('CLEAN','tables-zero')]:
 i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S846 '+tag+' ').encode()))
 bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^0x100000000000:016x}'.encode(),bad[i],count=1)
 assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
for tag in (b'READY',b'SWITCH',b'CLEAN',b'POLICY'):
 cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S846 '+tag+b' '))+b'\n')
cases.append(raw.replace(b'supervisor-switch=yes cpl3=no',b'supervisor-switch=yes cpl3=yes'))
for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
assert not verify(raw,0)[0]
print(f'S846 genuine-capture host refusals: {len(cases)+1} PASS')
