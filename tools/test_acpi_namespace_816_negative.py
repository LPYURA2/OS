#!/usr/bin/env python3
"""Realistic strict parser negatives; all DMA and namespace upgrade claims denied."""
from acpi_namespace_816_verify import verify,POLICY
from test_acpi_hostbridge_815_negative import sample as old_sample
H=lambda n:f'0x{n:016x}'
def sample(mode):
 old=old_sample(mode).decode('ascii')
 addon=(f'STYLEOS S816 NAMESPACE tables={H(1)} ssdts={H(0)} objects={H(0)} '
        f'complete={H(0)} literal-ready={H(0)} seg-known={H(0)} '
        f'bbn-known={H(0)} crs-known={H(0)} regions-denied={H(0)} '
        f'unsupported={H(1)} duplicates={H(0)}')
 old=old.replace('STYLEOS S815 AML ', 'STYLEOS S816 AML ')
 old=old.replace('STYLEOS S815 DSDTHEX',addon+'\nSTYLEOS S816 DSDTHEX',1)
 old=old.replace('STYLEOS S815 POLICY '+old.split('STYLEOS S815 POLICY ')[1].split('\n')[0],POLICY)
 old=old.replace('STYLEOS S815 ', 'STYLEOS S816 ')
 return old.encode('ascii')
count=0
def reject(mode,raw,rc=33):
 global count
 assert not verify(raw,rc,mode)[0],(mode,rc,raw[:200])
 count+=1
for mode in ('disabled','intel','bridge-intel','bridge-two-hop-intel','amd'):
 good=sample(mode)
 assert verify(good,33,mode)[0],mode
 mutations=[(good,0),(good,1),(good[:-1],33),
  (good+b'STYLEOS S816 PASS\n',33),
  (good+b'STYLEOS S816 FAIL forged\n',33),
  (good.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
  (good.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'),33),
  (good.replace(b'acpi-crs-evaluated=no',b'acpi-crs-evaluated=yes'),33),
  (good.replace(b'acpi-namespace-complete=no',b'acpi-namespace-complete=yes'),33),
  (good.replace(b'ssdt=checksummed-no-side-effects',b'ssdt=executed'),33),
  (good.replace(b'complete='+H(0).encode(),b'complete='+H(1).encode()),33),
  (good.replace(b'literal-ready='+H(0).encode(),b'literal-ready='+H(1).encode()),33),
  (good.replace(b'regions-denied='+H(0).encode(),b'regions-denied='+H(9000).encode()),33),
  (good.replace(b'objects='+H(0).encode(),b'objects='+H(129).encode()),33),
  (good.replace(b'tables='+H(1).encode(),b'tables='+H(9).encode()),33),
  (good.replace(b'ssdts='+H(0).encode(),b'ssdts='+H(1).encode()),33),
  (good.replace(b'unsupported='+H(1).encode(),b'unsupported='+H(20000).encode()),33),
  (good.replace(b'0x0000000000000020 data=',b'0x0000000000000021 data='),33),
  (good.replace((POLICY+'\n').encode(),b''),33),
  (good.replace(b'ecam-reservation-verified=no',b'ecam-reservation-verified=yes'),33)]
 for data,rc in mutations:reject(mode,data,rc)
print(f'STYLEOS S816 NEGATIVE PASS {count}/{count}')
