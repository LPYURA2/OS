#!/usr/bin/env python3
from iommu_810_verify import verify,POLICY
H=lambda x:f'0x{x:016x}'
def base(mode):
 n={'disabled':0,'intel':1,'amd':2}[mode]
 lines=[f'STYLEOS x86_64 UEFI entry',f'STYLEOS S810 PLATFORM backend={H(n)} root-entries={H(7)}']
 if mode=='intel':lines +=[f'STYLEOS S810 INTEL drhd={H(1)} rmrr={H(0)} unknown={H(0)}',f'STYLEOS S810 UNIT type={H(0)} index={H(0)} segment={H(0)} base={H(0xfed90000)} scope-count={H(7)}']
 if mode=='amd':lines +=[f'STYLEOS S810 AMD ivhd={H(1)} ivmd={H(0)} unknown={H(0)} opaque-scopes={H(1)}',f'STYLEOS S810 UNIT type={H(0x10)} index={H(0)} segment={H(0)} base={H(0xfed80000)} scope-count={H(0)}']
 lines +=[POLICY,'STYLEOS S810 PASS'];return ('\n'.join(lines)+'\n').encode()
tests=[]
def bad(label,mode,raw,rc=33):
 ok,_=verify(raw,rc,mode);assert not ok,label;tests.append(label)
for m in ('disabled','intel','amd'):
 assert verify(base(m),33,m)[0]
 bad(m+' wrong-exit',m,base(m),1)
 bad(m+' wrong-backend',m,base('disabled' if m!='disabled' else 'intel'))
 bad(m+' duplicated-pass',m,base(m)+b'STYLEOS S810 PASS\n')
 bad(m+' missing-policy',m,base(m).replace((POLICY+'\n').encode(),b''))
 bad(m+' unsafe-authorize',m,base(m).replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'))
 bad(m+' forged-cr3',m,base(m).replace(b'production-cr3=deny',b'production-cr3=allow'))
 bad(m+' injected-FAIL',m,base(m)+b'STYLEOS S810 FAIL forged\n')
if True:
 m='intel'
 bad('intel wrong drhd count',m,base(m).replace(b'drhd=0x0000000000000001',b'drhd=0x0000000000000002'))
 bad('intel wrong mmio alignment',m,base(m).replace(b'base=0x00000000fed90000',b'base=0x00000000fed90001'))
 bad('intel malformed unit',m,base(m).replace(b'scope-count=',b'scope='))
 m='amd'
 bad('amd unknown type',m,base(m).replace(b'type=0x0000000000000010',b'type=0x0000000000000050'))
 bad('amd forged scope count',m,base(m).replace(b'scope-count=0x0000000000000000',b'scope-count=0x0000000000000065'))
print(f'STYLEOS S810 NEGATIVE PASS {len(tests)}/{len(tests)}')
