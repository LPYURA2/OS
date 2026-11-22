#!/usr/bin/env python3
"""Synthetic UART fixture tests only; cannot establish hardware evidence."""
from pci_aperture_795_verify import verify,POLICY

def line(label,items):
    return b'STYLEOS S795 '+label.encode()+b' '+b' '.join(
        k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in items.items())
g={'base':0x80000000,'bytes':4096000,'width':1280,'height':800,'uefi-overlaps':0}
p={'bus':0,'device':1,'function':0,'vendor':0x1234,'id':0x1111,'devices':6,
   'index':0,'width':32,'base':0x80000000}
s={'cmd-before':3,'cmd-restored':3,'mask-lo':0xff000008,'mask-hi':0,
   'decode-disabled':1,'registers-restored':1,'size':0x1000000,'end':0x81000000}
def fixture(gg=g,pp=p,ss=s):
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',
        line('GOP',gg),line('PCI',pp),line('SIZING',ss),POLICY,b'STYLEOS S795 PASS',b''])
assert verify(fixture(),33)[0]
n=0
def reject(raw,rc=33):
    global n
    ok,_=verify(raw,rc);assert not ok,(raw,rc);n+=1
for rc in (0,None,32,34):reject(fixture(),rc)
for raw in (fixture()+b'STYLEOS S795 PASS\n',fixture().replace(POLICY,b'STYLEOS S795 POLICY production-cr3=allow'),
    fixture().replace(b'STYLEOS S795 PASS',b'STYLEOS S795 FAIL'),
    fixture().replace(b'STYLEOS S795 PCI',b'STYLEOS S795 PCl')):
    reject(raw)
for source,mutations in ((g,({'base':0x80001000},{'bytes':0},{'uefi-overlaps':1})),
    (p,({'base':0x90000000},{'device':32},{'width':128},{'index':6})),
    (s,({'cmd-restored':0},{'decode-disabled':0},{'registers-restored':0},
        {'size':0},{'size':0x800000},{'end':0x82000000},
        {'mask-lo':0xff800008},{'mask-hi':1},{'size':0x2000000},
        {'cmd-before':0x10000}))):
    for mod in mutations:
        gg,pp,ss=[dict(x) for x in (g,p,s)]
        if source is g:gg.update(mod)
        if source is p:pp.update(mod)
        if source is s:ss.update(mod)
        reject(fixture(gg,pp,ss))
print('STYLEOS S795 NEGATIVE PASS',n)
