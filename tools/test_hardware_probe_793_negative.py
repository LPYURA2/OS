#!/usr/bin/env python3
"""Parser mutants only; synthetic valid logs are never proof of real firmware."""
from hardware_probe_793_verify import FIELDS,POLICY,verify

def record(kind,values):
    return b'STYLEOS S793 '+kind.encode()+b' '+b' '.join(
        k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in zip(FIELDS[kind],values))

def fixture():
    g=[0x80000000,0x200000,640,480,640,1,0x80000000,0x80200000]
    m=[100,1,0x200000,1,0,11,1,2]
    c=[0x80010033,0x7801000,2,1<<5|1<<12,0x0007040600070406,0xC06,1]
    p=[0x3f8,0x60,0x64,0x1f0,0xfe,0xff,0]
    raw=b'STYLEOS x86_64 ExitBootServices OK\n'+b'\n'.join(
        record(kind,x) for kind,x in zip(FIELDS,(g,m,c,p)))+b'\n'+POLICY+b'\nSTYLEOS S793 PASS\n'
    return raw

def main():
    raw=fixture();assert verify(raw,33)[0],verify(raw,33)
    muts=[(raw,34),(raw.replace(b'STYLEOS S793 PASS',b'STYLEOS S793 FAIL'),33),
          (raw+raw,33),(raw.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
          (raw.replace(b'cache-effective=unknown',b'cache-effective=wc'),33)]
    for field in [b'base',b'bytes',b'page-start',b'page-end',b'overlaps',b'complete',
                  b'covered',b'cr3',b'pat',b'mtrr-supported',
                  b'com1',b'pic-master-mask',b'if']:
        import re
        pat=rb'('+field+rb'=0x)([0-9a-f]{16})'
        found=re.search(pat,raw)
        assert found,field
        replacement=found.group(1)+(b'0000000000000000' if field in (b'covered',b'pat',b'mtrr-supported',b'base',b'bytes',b'overlaps',b'cr3') else b'ffffffffffffffff')
        muts.append((raw[:found.start()]+replacement+raw[found.end():],33))
    # A real firmware may omit a PCI GOP BAR from GetMemoryMap. The parser must
    # preserve that gap as explicit evidence and NEVER claim full coverage.
    gapped=raw.replace(b'overlaps=0x0000000000000001',b'overlaps=0x0000000000000000')
    gapped=gapped.replace(b'covered=0x0000000000200000',b'covered=0x0000000000000000')
    gapped=gapped.replace(b'complete=0x0000000000000001',b'complete=0x0000000000000000')
    gapped=gapped.replace(b'type=0x000000000000000b',b'type=0x0000000000000000')
    gapped=gapped.replace(b'attributes=0x0000000000000001',b'attributes=0x0000000000000000')
    gapped=gapped.replace(b'STYLEOS S793 PASS',b'STYLEOS S793 GAP framebuffer-descriptor-absent\nSTYLEOS S793 PASS')
    assert verify(gapped,33)[0],verify(gapped,33)
    muts.append((gapped.replace(b'STYLEOS S793 GAP framebuffer-descriptor-absent\n',b''),33))
    muts.append((gapped.replace(b'complete=0x0000000000000000',b'complete=0x0000000000000001'),33))
    muts.append((raw.replace(b'STYLEOS S793 PASS',b'STYLEOS S793 GAP framebuffer-descriptor-absent\nSTYLEOS S793 PASS'),33))
    for bad,rc in muts:assert not verify(bad,rc)[0],(rc,bad)
    print(f'STYLEOS 793 UART negative tests: {len(muts)} corrupted fixtures rejected; not hardware evidence')
if __name__=='__main__':main()
