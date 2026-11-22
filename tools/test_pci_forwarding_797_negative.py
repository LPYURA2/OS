#!/usr/bin/env python3
"""Host-only synthetic negative tests. Never count as hardware proof."""
from pci_topology_796_verify import POLICY
from pci_forwarding_797 import ForwardingError,audit

def rec(name,**vals):
    return b'STYLEOS S796 '+name.encode()+b' '+b' '.join(
        k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in vals.items())
def device(bus,dev,cls,header):
    return rec('DEVICE',bus=bus,device=dev,function=0,vendor=0x1234,id=0x1111,
               **{'class':cls,'subclass':4 if cls==6 else 0,'header':header})
bridge=rec('BRIDGE',primary=0,secondary=1,subordinate=1,**{
    'memory-base':0x81000000,'memory-end':0x81200000,
    'prefetch-base':0xc000000000,'prefetch-end':0xc020000000})
gop=rec('GOP',base=0x80000000,bytes=4096000,bus=0,device=1,function=0,**{'observed-nonzero':1})
root=rec('BAR',bus=0,device=1,function=0,index=0,memory=1,base=0x80000000,width=32,prefetch=1)
inventory=rec('INVENTORY',buses=2,devices=3,bridges=1,routes=1,**{
    'bridge-windows':1,'memory-bars':3,'io-bars':0,'bar-sizes-unknown':3,
    'display-matches':1})
def child(base,prefetch,idx):
    return rec('BAR',bus=1,device=0,function=0,index=idx,memory=1,
               base=base,width=64 if prefetch else 32,prefetch=prefetch)
def sample(memory=0x81000000,prefetch=0xc000000000):
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',
        device(0,1,3,0),root,device(0,0x1c,6,1),bridge,
        device(1,0,3,0),child(memory,0,1),child(prefetch,1,4),
        inventory,gop,POLICY,b'STYLEOS S796 PASS',b''])
assert audit(sample(),33,require_bridge=True)['routed_memory_bar_bases_in_bridge_windows']==2
n=0
for normal,prefetch in [(0x81200000,0xc000000000),(0x80ffffff,0xc000000000),
 (0x81000000,0xc020000000),(0x81000000,0xbfffffffff),
 (0x81000000,0xc030000000),(0,0xc000000000)]:
    try:audit(sample(normal,prefetch),33,require_bridge=True)
    except ForwardingError:n+=1
    else:raise AssertionError('unsafe BAR accepted')
for forged in [sample().replace(b'production-cr3=deny',b'production-cr3=allow'),
               sample()+b'STYLEOS S796 PASS\n',
               sample().replace(b'STYLEOS S796 PASS',b'STYLEOS S796 FAIL')]:
    try:audit(forged,33,require_bridge=True)
    except ForwardingError:n+=1
    else:raise AssertionError('forged UART accepted')
for rc in (0,32,34,None):
    try:audit(sample(),rc,require_bridge=True)
    except ForwardingError:n+=1
    else:raise AssertionError('wrong QEMU exit accepted')
assert n==13,n
print('STYLEOS S797 NEGATIVE PASS',n)
