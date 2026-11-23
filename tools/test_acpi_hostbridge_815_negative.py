#!/usr/bin/env python3
from acpi_hostbridge_815_verify import verify,POLICY
H=lambda n:f'0x{n:016x}'
def dsdt():
 b=bytearray(64);b[:4]=b'DSDT';b[4:8]=(64).to_bytes(4,'little');b[8]=2
 b[9]=(-sum(b))&255
 return bytes(b)
def sample(mode):
 backend={'disabled':0,'intel':1,'bridge-intel':1,'bridge-two-hop-intel':1,'amd':2}[mode]
 bridges={'disabled':0,'intel':0,'bridge-intel':1,'bridge-two-hop-intel':2,'amd':0}[mode]
 buses=bridges+1;functions=6+bridges;exact=0 if not backend else 5
 lines=[f'STYLEOS S815 PLATFORM backend={H(backend)} root-entries={H(7)}',
  f'STYLEOS S815 TOPOLOGY buses={H(buses)} functions={H(functions)} bridges={H(bridges)}',
  f'STYLEOS S815 MCFG allocations={H(1)} segment0={H(1)} other-segments={H(0)} covered-buses={H(buses)} missing-buses={H(0)} root-base={H(0xe0000000)} root-end={H(0xe0100000)}',
  f'STYLEOS S815 SCOPE units={H(int(bool(backend)))} exact={H(exact)} unresolved={H(0)} aliases={H(0)} exceptions={H(0)} segments-unresolved={H(0)} collisions={H(0)}',
  f'STYLEOS S815 PCI explicit-matches={H(exact)} unresolved={H(functions-exact)} alias-sources-present={H(0)}',
  f'STYLEOS S815 ROOTBRIDGE roots={H(1)} segment0={H(1)} other-segments={H(0)} bus-ranges={H(1)} covered-buses={H(buses)} unresolved-buses={H(0)}',
  f'STYLEOS S815 AML dsdt-bytes={H(64)} devices={H(1)} pci-hid={H(1)} motherboard-hid={H(1)} seg-static={H(1)} bbn-static={H(1)} crs-static={H(1)} crs-method={H(1)} ecam-literal={H(0)} ssdts={H(0)}',
  f'STYLEOS S815 DSDTHEX offset={H(0)} data={dsdt()[:32].hex()}',
  f'STYLEOS S815 DSDTHEX offset={H(32)} data={dsdt()[32:].hex()}',
  POLICY,'STYLEOS S815 PASS']
 return ('\n'.join(lines)+'\n').encode()
n=0
def reject(mode,data,rc=33):
 global n
 assert not verify(data,rc,mode)[0],(mode,rc,data)
 n+=1
for mode in ('disabled','intel','bridge-intel','bridge-two-hop-intel','amd'):
 s=sample(mode)
 assert verify(s,33,mode)[0]
 reject(mode,s,0);reject(mode,s,1)
 reject(mode,s+b'STYLEOS S815 PASS\n')
 reject(mode,s+b'STYLEOS S815 FAIL tamper\n')
 reject(mode,s[:-1])
 reject(mode,s.replace(b'production-cr3=deny',b'production-cr3=allow'))
 reject(mode,s.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'))
 reject(mode,s.replace(b'acpi-crs-evaluated=no',b'acpi-crs-evaluated=yes'))
 reject(mode,s.replace(b'acpi-aml=static-hints-only',b'acpi-aml=fully-interpreted'))
 reject(mode,s.replace(b'ecam-reservation-verified=no',b'ecam-reservation-verified=yes'))
 reject(mode,s.replace(b'0x0000000000000020 data=',b'0x0000000000000021 data='))
 reject(mode,s.replace(b'0x0000000000000040 devices=',b'0x0000000000000041 devices='))
 reject(mode,s.replace(b'0x0000000000000000 data=44534454',b'0x0000000000000000 data=44414454'))
 reject(mode,s.replace(b'root-base=0x00000000e0000000',b'root-base=0x00000000d0000000'))
 reject(mode,s.replace((POLICY+'\n').encode(),b''))
 reject(mode,s.replace(b'collisions='+H(0).encode(),b'collisions='+H(1).encode()))
 reject(mode,s.replace(b'dma-isolation-verified=no',b'dma-isolation-verified=yes'))
print(f'STYLEOS S815 NEGATIVE PASS {n}/{n}')
