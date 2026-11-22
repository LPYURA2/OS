#!/usr/bin/env python3
"""Forged records must not be mistaken for actual PCI hardware evidence."""
from pci_topology_796_verify import verify,POLICY

def field(name,**kv):
    return b'STYLEOS S796 '+name.encode()+b' '+b' '.join(k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in kv.items())
d=field('DEVICE',bus=0,device=1,function=0,vendor=0x1234,id=0x1111,**{'class':3,'subclass':0,'header':0})
b=field('BAR',bus=0,device=1,function=0,index=0,memory=1,base=0x80000000,width=32,prefetch=1)
i=field('INVENTORY',buses=1,devices=1,bridges=0,routes=0,**{'bridge-windows':0,'memory-bars':1,'io-bars':0,'bar-sizes-unknown':1,'display-matches':1})
g=field('GOP',base=0x80000000,bytes=4096000,bus=0,device=1,function=0,**{'observed-nonzero':1})
def frame(dev=d,bar=b,inventory=i,gop=g,policy=POLICY):
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',dev,bar,inventory,gop,policy,b'STYLEOS S796 PASS',b''])
assert verify(frame(),33)[0]
n=0
def reject(raw,rc=33,need=False):
    global n
    ok,_=verify(raw,rc,require_bridge=need)
    assert not ok,'accepted forged evidence';n+=1
for x in [None,0,32,34]:reject(frame(),x)
for x in [frame()+b'STYLEOS S796 PASS\n',frame().replace(POLICY,b'STYLEOS S796 POLICY production-cr3=allow'),
          frame().replace(b'STYLEOS S796 PASS',b'STYLEOS S796 FAIL'),
          frame().replace(b'bar-sizes-unknown=0x0000000000000001',b'bar-sizes-unknown=0x0000000000000000'),
          frame().replace(b'display-matches=0x0000000000000001',b'display-matches=0x0000000000000002'),
          frame().replace(b'bus=0x0000000000000000 device=0x0000000000000001',b'bus=0x0000000000000007 device=0x0000000000000001'),
          frame().replace(b'0x0000000080000000',b'0x0000000090000000',1),
          frame().replace(b'width=0x0000000000000020',b'width=0x0000000000000080'),
          frame().replace(b'header=0x0000000000000000',b'header=0x0000000000000003')]:reject(x)
reject(frame(),33,True)
print('STYLEOS S796 NEGATIVE PASS',n)
