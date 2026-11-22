#!/usr/bin/env python3
"""Counterfeit UART rejection is necessary, not a substitute for real QEMU."""
from cache_800_verify import verify,KEYS,POLICY

def record(kind,*values):
 return b'STYLEOS S800 '+kind.encode()+b' '+b' '.join(k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in zip(KEYS[kind],values))
# Eight inactive variable MTRRs, enabled default WB. No PAT/PTE proof.
rows=[record('MTRR',i,6,0) for i in range(8)]
raw=b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',
 record('CPU',0x80010033,0x7801000,0x0007040600070406,8,0x806,40,8),
 record('PCI',0x10431af4,0x80000000,0x81000000,0x0000000c,0x000000c0),
 *rows,
 record('TARGET',0,0x80000000,6,0,1),record('TARGET',1,0x81000000,6,0,1),
 record('TARGET',2,0xc00000000c,6,0,1),POLICY,b'STYLEOS S800 PASS'])+b'\n'
# BAR4 offset 0xC is masked; addresses are 0xC000000000.
raw=raw.replace(b'address=0x000000c00000000c',b'address=0x000000c000000000')
assert verify(raw,33)[0],verify(raw,33)
cases=[(raw,32),(raw.replace(b'PASS',b'FAIL'),33),
 (raw.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
 (raw.replace(b'effective-pat-pte=unknown',b'effective-pat-pte=wc'),33),
 (raw+raw,33),(raw.replace(b'STYLEOS S800 PASS\n',b''),33),
 (raw.replace(b'variable-count=0x0000000000000008',b'variable-count=0x0000000000000007'),33),
 (raw.replace(b'mtrr-type=0x0000000000000006',b'mtrr-type=0x0000000000000000',1),33),
 (raw.replace(b'bar1raw=0x0000000081000000',b'bar1raw=0x0000000082000000'),33),
 (raw.replace(b'physbits=0x0000000000000028',b'physbits=0x0000000000000010'),33),
 (raw.replace(b'pat=0x0007040600070406',b'pat=0x0007040600070402'),33),
 (raw.replace(b'index=0x0000000000000004',b'index=0x0000000000000003',1),33)]
for bad,code in cases:
 assert not verify(bad,code)[0],(code,bad)
print(f'STYLEOS 800 negative parser: {len(cases)} forged variants refused')
