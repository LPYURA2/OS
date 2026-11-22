#!/usr/bin/env python3
"""Non-hardware parser test suite; always requires real QEMU separately."""
from cache_801_verify import verify,KEYS,POLICY
def rec(kind,*vals):
 return b'STYLEOS S801 '+kind.encode()+b' '+b' '.join(k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in zip(KEYS[kind],vals))
base=b'\n'.join([
 b'STYLEOS x86_64 ExitBootServices OK',
 rec('CPU',0x80010033,0x7801000,0x0007040600070406,40),
 rec('TARGET',0,0x80000000,1,0x80000000,2097152,0x80000083,0,6),
 rec('TARGET',1,0x81000000,1,0x81000000,2097152,0x81000083,0,6),
 rec('TARGET',2,0xc000000000,0,0,0,0,0,0),
 POLICY,b'STYLEOS S801 PASS'])+b'\n'
assert verify(base,33)[0],verify(base,33)
changes=[(base,32),(base.replace(b'S801 PASS',b'S801 FAIL'),33),
 (base.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
 (base.replace(b'global-alias-scan=unverified',b'global-alias-scan=verified'),33),
 (base.replace(b'physbits=0x0000000000000028',b'physbits=0x0000000000000010'),33),
 (base.replace(b'present=0x0000000000000001',b'present=0x0000000000000002',1),33),
 (base.replace(b'page-size=0x0000000000200000',b'page-size=0x0000000000001000',1),33),
 (base.replace(b'pat-type=0x0000000000000006',b'pat-type=0x0000000000000000',1),33),
 (base.replace(b'pat-index=0x0000000000000000',b'pat-index=0x0000000000000001',1),33),
 (base.replace(b'cr3=0x0000000007801000',b'cr3=0x0000000000000000'),33),
 (base.replace(b'address=0x0000000081000000',b'address=0x0000000082000000'),33),
 (base.replace(b'translated=0x0000000081000000',b'translated=0x0000000082000000'),33),
 (base.replace(b'STYLEOS S801 PASS\n',b''),33),
 (base+base,33),
 (base.replace(b'present=0x0000000000000001',b'present=0x0000000000000000',2),33),
 (base.replace(b'effective-mmio-cache=unverified',b'effective-mmio-cache=verified'),33),
 (base.replace(b'entry=0x0000000080000083',b'entry=0x0000000080000003'),33)]
for variant,code in changes:
 assert not verify(variant,code)[0],verify(variant,code)
print(f'STYLEOS 801 negative parser PASS {len(changes)} rejected')
