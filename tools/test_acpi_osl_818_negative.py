"""S818 detects forged guest hardware permissions and malformed OSL records."""
from osl_818_verify import verify,POLICY
from test_acpi_namespace_816_negative import sample as legacy
H=lambda n:f'0x{n:016x}'
def sample(mode):
 old=legacy(mode).decode('ascii')
 pre=(f'STYLEOS S818 OSL dsdt-bytes={H(64)} snapshot-reads={H(1)} '
      f'region-denied={H(12)} budget={H(256)}\n'
      +POLICY+'\nSTYLEOS S818 PASS\n')
 return old.replace('STYLEOS S816 PLATFORM',pre+'STYLEOS S816 PLATFORM',1).encode('ascii')
count=0
for mode in ('disabled','intel','bridge-intel','bridge-two-hop-intel','amd'):
 good=sample(mode); ok,detail=verify(good,33,mode)
 assert ok,(mode,detail)
 changes=[(good,0),(good,1),(good[:-1],33),
  (good+b'STYLEOS S818 PASS\n',33),
  (good.replace(b'STYLEOS S818 PASS',b'STYLEOS S818 FAIL'),33),
  (good.replace(b'acpica-linked=no',b'acpica-linked=yes'),33),
  (good.replace(b'operationregion=deny-all',b'operationregion=allow-all'),33),
  (good.replace(b'hardware-io=none',b'hardware-io=available'),33),
  (good.replace(b'ecam=not-read',b'ecam=read'),33),
  (good.replace(b'dma-grants=no',b'dma-grants=yes'),33),
  (good.replace(b'acpi-namespace-evaluated=no',b'acpi-namespace-evaluated=yes'),33),
  (good.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
  (good.replace(f'region-denied={H(12)}'.encode(),f'region-denied={H(11)}'.encode()),33),
  (good.replace(f'region-denied={H(12)}'.encode(),f'region-denied={H(13)}'.encode()),33),
  (good.replace(f'snapshot-reads={H(1)}'.encode(),f'snapshot-reads={H(2)}'.encode()),33),
  (good.replace(f'budget={H(256)}'.encode(),f'budget={H(0)}'.encode()),33),
  (good.replace(f'budget={H(256)}'.encode(),f'budget={H(257)}'.encode()),33),
  (good.replace(f'dsdt-bytes={H(64)}'.encode(),f'dsdt-bytes={H(63)}'.encode()),33),
  (good.replace(POLICY.encode()+b'\n',b''),33),
  (good.replace(b'STYLEOS S818 PASS\n',b'',1),33),
  (good.replace(b'STYLEOS S816 POLICY',b'STYLEOS S816 NO_POLICY'),33)]
 for raw,rc in changes:
  assert not verify(raw,rc,mode)[0],(mode,rc,raw[:300]);count+=1
print(f'STYLEOS S818 NEGATIVE PASS {count}/{count}')
