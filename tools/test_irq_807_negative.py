"""Fail-closed UART mutations. No synthetic hardware log accepted as actual QEMU evidence."""
from irq_807_verify import POLICY,verify
from pathlib import Path
base=b'STYLEOS x86_64 ExitBootServices OK\n'+b'\n'.join([
 b'STYLEOS S807 TOPOLOGY ioapic-physical=0x00000000fec00000 ioapic-id=0x0000000000000000 ioapic-count=0x0000000000000018 gsi=0x0000000000000002 pin=0x0000000000000002 lapic-physical=0x00000000fee00000 lapic-id=0x0000000000000000',
 b'STYLEOS S807 BEFORE ioapic-low=0x0000000000010000 ioapic-high=0x0000000000000000 tpr=0x0000000000000000 svr=0x00000000000001ff if=0x0000000000000000',
 b'STYLEOS S807 ROUTE vector=0x0000000000000050 ioapic-low=0x0000000000000050 ioapic-high=0x0000000000000000 gsi=0x0000000000000002',
 b'STYLEOS S807 DELIVERY hits=0x0000000000000001 isr-seen=0x0000000000000001 eoi-cleared=0x0000000000000001 elapsed-tsc=0x0000000000100000',
 b'STYLEOS S807 RESTORE ioapic-low=0x0000000000010000 ioapic-high=0x0000000000000000 tpr=0x0000000000000000 svr=0x00000000000001ff if=0x0000000000000000',
 POLICY,b'STYLEOS S807 PASS'])+b'\n'
ok,data=verify(base,33);assert ok,data
mutations=[
 base.replace(b'gsi=0x0000000000000002',b'gsi=0x0000000000000003',1),
 base.replace(b'pin=0x0000000000000002',b'pin=0x0000000000000018',1),
 base.replace(b'ioapic-physical=0x00000000fec00000',b'ioapic-physical=0x00000000fec00001',1),
 base.replace(b'ioapic-count=0x0000000000000018',b'ioapic-count=0x0000000000000000',1),
 base.replace(b'ioapic-low=0x0000000000000050',b'ioapic-low=0x0000000000010050',1),
 base.replace(b'vector=0x0000000000000050',b'vector=0x0000000000000051',1),
 base.replace(b'hits=0x0000000000000001',b'hits=0x0000000000000000',1),
 base.replace(b'isr-seen=0x0000000000000001',b'isr-seen=0x0000000000000000',1),
 base.replace(b'eoi-cleared=0x0000000000000001',b'eoi-cleared=0x0000000000000000',1),
 base.replace(b'elapsed-tsc=0x0000000000100000',b'elapsed-tsc=0x0000000000000000',1),
 base.replace(b'ioapic-low=0x0000000000010000',b'ioapic-low=0x0000000000010001',1),
 base.replace(b'tpr=0x0000000000000000',b'tpr=0x0000000000000080',1),
 base.replace(b'if=0x0000000000000000',b'if=0x0000000000000001',1),
 base.replace(b'pit-restored=no',b'pit-restored=yes'),
 base.replace(b'production-cr3=deny',b'production-cr3=allow'),
 base.replace(b'ordinary-desktop=unchanged',b'ordinary-desktop=changed'),
 base.replace(b'STYLEOS S807 PASS',b'STYLEOS S807 FAIL device'),
 base+b'STYLEOS S807 PASS\n',
 base.replace(b'STYLEOS S807 PASS',b'STYLEOS S807 ROUTE vector=0x0000000000000050\nSTYLEOS S807 PASS'),
 base.replace(b'STYLEOS x86_64 ExitBootServices OK\n',b''),
]
for i,m in enumerate(mutations):
 ok,detail=verify(m,33);assert not ok,(i,detail)
assert not verify(base,0)[0]
assert not verify(base,124)[0]
print('STYLEOS S807 strict counterfeit negatives PASS',len(mutations)+2)
