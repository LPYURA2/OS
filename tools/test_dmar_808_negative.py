#!/usr/bin/env python3
"""Reject fabricated DMA protection, missing DMAR, malformed scope and UART."""
from dmar_808_verify import verify
hx=lambda n:'0x'+format(n,'016x')
S='STYLEOS S808 STATUS present='+hx(1)+' root-entries='+hx(7)
D='STYLEOS S808 DMAR length='+hx(64)+' revision='+hx(1)+' host-aw='+hx(39)+' flags='+hx(1)+' drhd-count='+hx(1)+' rmrr-count='+hx(0)+' unknown-count='+hx(0)
H='STYLEOS S808 DRHD index='+hx(0)+' segment='+hx(0)+' flags='+hx(1)+' base='+hx(0xfed90000)+' scope-count='+hx(0)
P='STYLEOS S808 POLICY dmar-discovery-verified=yes iommu-registers-observed=no iommu-translation-enabled=no dma-isolation-verified=no dma-access-authorized=no uefi-runtime-verified=no production-cr3=deny'
PASS='STYLEOS S808 PASS'
def sample(lines):return ('STYLEOS x86_64 UEFI entry\n'+'\n'.join(lines)+'\n').encode()
valid=sample([S,D,H,P,PASS]);off=sample(['STYLEOS S808 STATUS present='+hx(0)+' root-entries='+hx(6),P,PASS])
assert verify(valid,33,True)[0] and verify(off,33,False)[0]
bad=[
 (valid,0,True),(valid,33,False),(off,33,True),(valid.replace(b'STYLEOS S808 PASS',b''),33,True),
 (valid.replace(b'production-cr3=deny',b'production-cr3=allow'),33,True),
 (valid.replace(b'dma-isolation-verified=no',b'dma-isolation-verified=yes'),33,True),
 (valid.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'),33,True),
 (valid.replace(b'iommu-translation-enabled=no',b'iommu-translation-enabled=yes'),33,True),
 (valid.replace(b'iommu-registers-observed=no',b'iommu-registers-observed=yes'),33,True),
 (valid.replace(b'drhd-count='+hx(1).encode(),b'drhd-count='+hx(2).encode()),33,True),
 (valid.replace(b'base='+hx(0xfed90000).encode(),b'base='+hx(0).encode()),33,True),
 (valid.replace(b'base='+hx(0xfed90000).encode(),b'base='+hx(0xfed90001).encode()),33,True),
 (valid.replace(b'root-entries='+hx(7).encode(),b'root-entries='+hx(0).encode()),33,True),
 (valid.replace(b'present='+hx(1).encode(),b'present='+hx(2).encode()),33,True),
 (valid.replace(b'scope-count='+hx(0).encode(),b'scope-count='+hx(65).encode()),33,True),
 (valid.replace(b'flags='+hx(1).encode()+b' base=',b'flags='+hx(2).encode()+b' base='),33,True),
 (valid.replace(b'STYLEOS S808 DRHD',b'STYLEOS S808 RMRR'),33,True),
 (valid.replace(b'STYLEOS S808 DRHD',b'STYLEOS S808 DRHD invalid=0x0000000000000000'),33,True),
 (valid.replace(b'STYLEOS S808 PASS',b'STYLEOS S808 PASS\nSTYLEOS S808 PASS'),33,True),
 (valid.replace(b'STYLEOS S808 PASS',b'STYLEOS S808 FAIL forged\nSTYLEOS S808 PASS'),33,True),
 (valid.replace(b'host-aw='+hx(39).encode(),b'host-aw='+hx(17).encode()),33,True),
 (valid.replace(b'revision='+hx(1).encode(),b'revision=foobar'),33,True),
]
for i,(data,rc,expected) in enumerate(bad):
 ok,obs=verify(data,rc,expected)
 if ok:raise AssertionError(f'negative case {i} unexpectedly accepted: {obs}')
print(f'S808 negative UART test PASS {len(bad)}/{len(bad)}')
