"""Negative replay of genuine guest capture (host-only mutations)."""
import argparse,re
from pathlib import Path
from apic_root_842_verify import verify
def main():
    p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
    raw=a.input.read_bytes();assert verify(raw,33)[0]
    lines=raw.splitlines();cases=[]
    for tag,key in [('READY','new'),('READY','uc-index'),('READY','lapic'),('IRQ','during'),('IRQ','frame'),('IRQ','ist')]:
        i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S842 '+tag+' ').encode()))
        bad=list(lines);bad[i]=re.sub((key+r'=0x([0-9a-f]{16})').encode(),lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^0x1000:016x}'.encode(),bad[i],count=1)
        assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
    for tag in [b'READY',b'IRQ',b'POLICY']:
        cases.append(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S842 '+tag+b' '))+b'\n')
    cases.append(raw.replace(b'production-cr3=deny dma-grants=no',b'production-cr3=allow dma-grants=no'))
    for i,bad in enumerate(cases):assert not verify(bad,33)[0],i
    assert not verify(raw,0)[0]
    print(f'S842 genuine-capture host refusals: {len(cases)+1} PASS')
if __name__=='__main__':main()
