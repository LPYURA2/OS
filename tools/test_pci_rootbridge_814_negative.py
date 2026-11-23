#!/usr/bin/env python3
"""S814 false evidence must never be accepted (including forged MCFG coverage)."""
from pci_rootbridge_814_verify import verify,POLICY
H=lambda n:f'0x{n:016x}'
BACKEND={'disabled':0,'intel':1,'amd':2,'bridge-intel':1,'bridge-two-hop-intel':1}
def fixture(mode):
 backend=BACKEND[mode]
 bridges={'bridge-intel':1,'bridge-two-hop-intel':2}.get(mode,0)
 buses=bridges+1
 exact=int(backend!=0)
 lines=['STYLEOS x86_64 UEFI entry',
  f'STYLEOS S814 PLATFORM backend={H(backend)} root-entries={H(7)}',
  f'STYLEOS S814 TOPOLOGY buses={H(buses)} functions={H(5)} bridges={H(bridges)}',
  f'STYLEOS S814 MCFG allocations={H(1)} segment0={H(1)} other-segments={H(0)} covered-buses={H(buses)} missing-buses={H(0)} root-base={H(0xb0000000)} root-end={H(0xb0100000)}',
  f'STYLEOS S814 SCOPE units={H(exact)} exact={H(exact)} unresolved={H(0)} aliases={H(0)} exceptions={H(0)} segments-unresolved={H(0)} collisions={H(0)}',
  f'STYLEOS S814 PCI explicit-matches={H(exact)} unresolved={H(5-exact)} alias-sources-present={H(0)}',
  f'STYLEOS S814 ROOTBRIDGE roots={H(1)} segment0={H(1)} other-segments={H(0)} bus-ranges={H(1)} covered-buses={H(buses)} unresolved-buses={H(0)}',
  POLICY,'STYLEOS S814 PASS']
 return ('\n'.join(lines)+'\n').encode()
rejected=0
def reject(mode,raw,code=33):
 global rejected
 assert not verify(raw,code,mode)[0],(mode,raw,code)
 rejected+=1
for mode in BACKEND:
 good=fixture(mode);assert verify(good,33,mode)[0]
 reject(mode,good,1);reject(mode,good,0);reject(mode,good+b'STYLEOS S814 PASS\n')
 reject(mode,good+b'STYLEOS S814 FAIL forged\n');reject(mode,good[:-1])
 reject(mode,good.replace(b'uefi-rootbridge-resources=read-only',b'uefi-rootbridge-resources=read-write'))
 reject(mode,good.replace(b'ecam-dereference=no',b'ecam-dereference=yes'))
 reject(mode,good.replace(b'acpi-crs-evaluated=no',b'acpi-crs-evaluated=yes'))
 reject(mode,good.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'))
 reject(mode,good.replace(b'production-cr3=deny',b'production-cr3=allow'))
 reject(mode,good.replace(b'acpi-seg-evaluated=no',b'acpi-seg-evaluated=yes'))
 reject(mode,good.replace(b'collisions='+H(0).encode(),b'collisions='+H(1).encode()))
 reject(mode,good.replace(b'missing-buses='+H(0).encode(),b'missing-buses='+H(1).encode()))
 buses={'bridge-intel':2,'bridge-two-hop-intel':3}.get(mode,1)
 reject(mode,good.replace(b'covered-buses='+H(buses).encode(),b'covered-buses='+H(buses-1).encode()))
 reject(mode,good.replace(b'root-base='+H(0xb0000000).encode(),b'root-base='+H(0).encode()))
 reject(mode,good.replace(b'root-end='+H(0xb0100000).encode(),b'root-end='+H(0xb0200000).encode()))
 reject(mode,good.replace(b'segment0='+H(1).encode(),b'segment0='+H(0).encode()))
 reject(mode,good.replace(b'allocations='+H(1).encode(),b'allocations='+H(33).encode()))
 reject(mode,good.replace((POLICY+'\n').encode(),b''))
 reject(mode,good.replace(b'bus-ranges='+H(1).encode(),b'bus-ranges='+H(0).encode()))
 reject(mode,good.replace(b'roots='+H(1).encode(),b'roots='+H(0).encode()))
 reject(mode,good.replace(b'unresolved-buses='+H(0).encode(),b'unresolved-buses='+H(1).encode()))
 reject(mode,good.replace(b'uefi-segment0-verified=bounded',b'uefi-segment0-verified=complete'))
print(f'STYLEOS S814 NEGATIVE PASS {rejected}/{rejected}')
