#!/usr/bin/env python3
"""Refuse forged/unsafe S799 hardware records."""
from pci_range_799_verify import verify
P=b'STYLEOS '
def line(tag,**values):
    return P+b'S799 '+tag.encode()+b' '+b' '.join(k.encode()+b'=0x'+format(v,'016x').encode() for k,v in values.items())
def good():
    header=b'STYLEOS x86_64 ExitBootServices OK'
    ident=line('PCI',bus=1,device=0,function=0,vendor=0x1af4,id=0x1043,
       **{'bridge-bus':0,'bridge-device':0x1c,'secondary':1})
    state=line('STATE',**{'command-before':7,'command-after':7,'decode-disabled':1,'registers-restored':1})
    bar1=line('BAR',index=1,width=32,prefetch=0,**{'raw-lo':0x81000000,'raw-hi':0,'mask-lo':0xfffff000,'mask-hi':0,
       'base':0x81000000,'size':4096,'end':0x81001000,'window-base':0x81000000,'window-end':0x81200000})
    bar4=line('BAR',index=4,width=64,prefetch=1,**{'raw-lo':12,'raw-hi':0xc0,'mask-lo':0xffffc00c,'mask-hi':0xffffffff,
       'base':0xc000000000,'size':16384,'end':0xc000004000,'window-base':0xc000000000,'window-end':0xc020000000})
    policy=b'STYLEOS S799 POLICY range-proven=two-downstream-bars cache-effective=unknown all-bars=unmeasured irq-routing=unknown firmware-runtime=unknown production-cr3=deny'
    return b'\n'.join((header,ident,state,bar1,bar4,policy,b'STYLEOS S799 PASS',b''))
def main():
    original=good()
    assert verify(original,33)[0],verify(original,33)
    mutations=[
      (original,1), (original,0), (original,35),
      (original.replace(b'STYLEOS S799 PASS',b'STYLEOS S799 FAIL bad'),33),
      (original.replace(b'command-after=0x0000000000000007',b'command-after=0x0000000000000006'),33),
      (original.replace(b'decode-disabled=0x0000000000000001',b'decode-disabled=0x0000000000000000'),33),
      (original.replace(b'registers-restored=0x0000000000000001',b'registers-restored=0x0000000000000000'),33),
      (original.replace(b'secondary=0x0000000000000001',b'secondary=0x0000000000000002'),33),
      (original.replace(b'vendor=0x0000000000001af4',b'vendor=0x0000000000001234'),33),
      (original.replace(b'mask-lo=0x00000000fffff000',b'mask-lo=0x00000000ffffe000'),33),
      (original.replace(b'mask-hi=0x00000000ffffffff',b'mask-hi=0x00000000fffffffe'),33),
      (original.replace(b'size=0x0000000000001000',b'size=0x0000000000002000'),33),
      (original.replace(b'window-end=0x0000000081200000',b'window-end=0x0000000081000800'),33),
      (original.replace(b'raw-lo=0x0000000081000000',b'raw-lo=0x0000000081001000'),33),
      (original.replace(b'index=0x0000000000000004',b'index=0x0000000000000005'),33),
      (original.replace(b'width=0x0000000000000040',b'width=0x0000000000000020'),33),
      (original.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
      (original.replace(b'STYLEOS S799 PASS',b'STYLEOS S799 PASS\nSTYLEOS S799 PASS'),33),
      (original.replace(b'STYLEOS S799 POLICY',b'STYLEOS S799 POLICY\nSTYLEOS S799 POLICY'),33),
      (original.replace(b'STYLEOS S799 BAR',b'STYLEOS S799 BAR bad=0x0000000000000001',1),33),
      (original.replace(b'STYLEOS x86_64 ExitBootServices OK',b'STYLEOS x86_64 boot (not exited)'),33),
      (original.replace(b'cache-effective=unknown',b'cache-effective=verified'),33),
      (original.replace(b'command-before=0x0000000000000007',b'command-before=0x0000000000000000'),33),
    ]
    for i,(raw,rc) in enumerate(mutations):
        assert not verify(raw,rc)[0],f'counterfeit accepted: {i} {verify(raw,rc)}'
    print('STYLEOS S799 NEGATIVE PASS',len(mutations))
if __name__=='__main__':main()
