#!/usr/bin/env python3
from vtd_809_verify import verify
h=lambda n:'0x'+f'{n:016x}'
def row(tag,**kw):return 'STYLEOS S809 '+tag+' '+' '.join(k.replace('_','-')+'='+h(v) for k,v in kw.items())
S=row('STATUS',present=1,drhd_count=1)
M=row('MMIO',base=0xfed90000,mapped=1,physical=0xfed90000,page_size=2097152,cr3=0x7801000)
R=row('REGS',ver=0x10,major=1,minor=0,cap=0x1234,ecap=0x5678,gcmd=0,gsts=0,rtaddr=0,fsts=0)
T=row('STATE',translation_status=0,translation_command=0,root_set_status=0)
P='STYLEOS S809 POLICY register-snapshot=observed dma-isolation-verified=no dma-access-authorized=no iommu-domains-verified=no uefi-runtime-verified=no production-cr3=deny'
PASS='STYLEOS S809 PASS'
def data(lines):return ('STYLEOS x86_64 UEFI entry\n'+'\n'.join(lines)+'\n').encode()
v=data([S,M,R,T,P,PASS]);off=data([row('STATUS',present=0,drhd_count=0),P,PASS])
assert verify(v,33,True)[0] and verify(off,33,False)[0]
changes=[
(v,0,True),(v,33,False),(off,33,True),(v.replace(b'STYLEOS S809 PASS',b''),33,True),
(v.replace(b'production-cr3=deny',b'production-cr3=allow'),33,True),
(v.replace(b'dma-isolation-verified=no',b'dma-isolation-verified=yes'),33,True),
(v.replace(b'iommu-domains-verified=no',b'iommu-domains-verified=yes'),33,True),
(v.replace(b'dma-access-authorized=no',b'dma-access-authorized=yes'),33,True),
(v.replace(b'base='+h(0xfed90000).encode(),b'base='+h(0).encode()),33,True),
(v.replace(b'physical='+h(0xfed90000).encode(),b'physical='+h(0xfed91000).encode()),33,True),
(v.replace(b'mapped='+h(1).encode(),b'mapped='+h(0).encode()),33,True),
(v.replace(b'ver='+h(0x10).encode(),b'ver='+h(0).encode()),33,True),
(v.replace(b'cap='+h(0x1234).encode(),b'cap='+h(0).encode()),33,True),
(v.replace(b'major='+h(1).encode(),b'major='+h(2).encode()),33,True),
(v.replace(b'translation-status='+h(0).encode(),b'translation-status='+h(1).encode()),33,True),
(v.replace(b'root-set-status='+h(0).encode(),b'root-set-status='+h(1).encode()),33,True),
(v.replace(b'STYLEOS S809 PASS',b'STYLEOS S809 PASS\nSTYLEOS S809 PASS'),33,True),
(v.replace(b'STYLEOS S809 PASS',b'STYLEOS S809 FAIL forged\nSTYLEOS S809 PASS'),33,True),
(v.replace(b'STYLEOS S809 REGS ',b'STYLEOS S809 REGX '),33,True),
(v.replace(b'gsts='+h(0).encode(),b'gsts='+h(0x80000000).encode()),33,True),
(v.replace(b'page-size='+h(2097152).encode(),b'page-size='+h(0).encode()),33,True),
(v.replace(b'STYLEOS S809 STATE',b'STYLEOS S809 MMIO'),33,True),
]
for i,(raw,rc,enabled) in enumerate(changes):
 if verify(raw,rc,enabled)[0]:raise AssertionError(f'accepted forgery {i}')
print(f'S809 negative VT-d UART PASS {len(changes)}/{len(changes)}')
