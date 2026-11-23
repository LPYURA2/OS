#!/usr/bin/env python3
from pci_requester_811_verify import verify,POLICY
H=lambda n:f'0x{n:016x}'
def make(mode):
 v={'disabled':0,'intel':1,'amd':2}[mode]
 lines=['STYLEOS x86_64 UEFI entry',f'STYLEOS S811 PLATFORM backend={H(v)} root-entries={H(7)}',
        f'STYLEOS S811 SCOPE units={H(int(v!=0))} exact={H(int(v!=0))} unresolved={H(0)} include-all={H(0)} ranges={H(0)} collisions={H(0)}',
        f'STYLEOS S811 PCI bus={H(0)} present={H(5)} explicit-matches={H(int(v!=0))} unresolved={H(5-int(v!=0))}',
        POLICY,'STYLEOS S811 PASS']
 return ('\n'.join(lines)+'\n').encode()
n=0
def reject(mode,raw,rc=33):
 global n
 assert not verify(raw,rc,mode)[0],(mode,raw)
 n+=1
for mode in ('disabled','intel','amd'):
 original=make(mode)
 assert verify(original,33,mode)[0]
 reject(mode,original,1);reject(mode,original,0)
 reject(mode,original+b'STYLEOS S811 PASS\n')
 reject(mode,original+b'STYLEOS S811 FAIL tamper\n')
 reject(mode,original.replace(b'production-cr3=deny',b'production-cr3=allow'))
 reject(mode,original.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'))
 reject(mode,original.replace(b'device-ownership-complete=no',b'device-ownership-complete=yes'))
 reject(mode,original.replace(b'bridge-hierarchy-verified=no',b'bridge-hierarchy-verified=yes'))
 reject(mode,original.replace(b'collisions=0x0000000000000000',b'collisions=0x0000000000000001'))
 reject(mode,original.replace(b'present=0x0000000000000005',b'present=0x0000000000000000'))
 reject(mode,original.replace(b'bus=0x0000000000000000',b'bus=0x0000000000000001'))
 reject(mode,original.replace((POLICY+'\n').encode(),b''))
 reject(mode,original[:-1])
 reject(mode,original.replace(b'exact=0x0000000000000001',b'exact=0x0000000000000101')) if mode!='disabled' else reject(mode,original.replace(b'units=0x0000000000000000',b'units=0x0000000000000001'))
print(f'STYLEOS S811 NEGATIVE PASS {n}/{n}')
