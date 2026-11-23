#!/usr/bin/env python3
from pci_requester_812_verify import verify,POLICY
H=lambda n:f'0x{n:016x}'
def sample(mode):
 backend={'disabled':0,'intel':1,'amd':2,'bridge-intel':1,'bridge-two-hop-intel':1}[mode]
 buses=3 if mode=='bridge-two-hop-intel' else (2 if mode=='bridge-intel' else 1)
 bridges=buses-1
 exact=int(backend!=0)
 lines=['STYLEOS x86_64 UEFI entry',
  f'STYLEOS S812 PLATFORM backend={H(backend)} root-entries={H(7)}',
  f'STYLEOS S812 TOPOLOGY buses={H(buses)} functions={H(5)} bridges={H(bridges)}',
  f'STYLEOS S812 SCOPE units={H(exact)} exact={H(exact)} unresolved={H(0)} bridge-scopes={H(0)} aliases={H(0)} extended={H(0)} include-all={H(0)} exceptions={H(0)} segments-unresolved={H(0)} collisions={H(0)}',
  f'STYLEOS S812 PCI explicit-matches={H(exact)} unresolved={H(5-exact)} alias-sources-present={H(0)}',
  POLICY,'STYLEOS S812 PASS']
 return ('\n'.join(lines)+'\n').encode()
count=0
def deny(mode,raw,rc=33):
 global count
 assert not verify(raw,rc,mode)[0],(mode,raw,rc)
 count+=1
for mode in ('disabled','intel','amd','bridge-intel','bridge-two-hop-intel'):
 good=sample(mode);assert verify(good,33,mode)[0]
 deny(mode,good,1);deny(mode,good,0)
 deny(mode,good+b'STYLEOS S812 PASS\n')
 deny(mode,good+b'STYLEOS S812 FAIL forged\n')
 deny(mode,good[:-1])
 deny(mode,good.replace(b'production-cr3=deny',b'production-cr3=allow'))
 deny(mode,good.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'))
 deny(mode,good.replace(b'dma-isolation-verified=no',b'dma-isolation-verified=yes'))
 deny(mode,good.replace(b'device-ownership-complete=no',b'device-ownership-complete=yes'))
 deny(mode,good.replace(b'pci-bridge-walk=bounded',b'pci-bridge-walk=complete'))
 deny(mode,good.replace(b'collisions=0x0000000000000000',b'collisions=0x0000000000000001'))
 deny(mode,good.replace(b'functions=0x0000000000000005',b'functions=0x0000000000000000'))
 deny(mode,good.replace(b'bridges='+H(1).encode(),b'bridges='+H(0).encode())) if mode in ('bridge-intel','bridge-two-hop-intel') else deny(mode,good.replace(b'buses='+H(1).encode(),b'buses='+H(0).encode()))
 deny(mode,good.replace(b'unresolved='+H(5-('disabled'!=mode)).encode(),b'unresolved='+H(0).encode()))
 deny(mode,good.replace((POLICY+'\n').encode(),b''))
print(f'STYLEOS S812 NEGATIVE PASS {count}/{count}')
