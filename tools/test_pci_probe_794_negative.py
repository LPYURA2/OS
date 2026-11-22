#!/usr/bin/env python3
"""Synthetic UART fixtures are STRICTLY negative/parser tests, not hardware evidence."""
from pci_probe_794_verify import verify,POLICY

def line(label,fields):
    return b'STYLEOS S794 '+label.encode()+b' '+b' '.join(
        k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in fields.items())

g={'base':0x80000000,'bytes':4096000,'width':1280,'height':800,
   'descriptors':128,'uefi-overlaps':0}
s={'bus':0,'devices':6,'display':1,'match':1}
p={'bus':0,'device':1,'function':0,'vendor':0x1234,'id':0x1111,
   'class':3,'subclass':0,'command':3}
b={'index':0,'raw-lo':0x80000008,'raw-hi':0,
   'base':0x80000000,'width':32,'prefetch':1}

def fixture(g=g,s=s,p=p,b=b,policy=POLICY):
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',line('GOP',g),
        line('SCAN',s),line('PCI',p),line('BAR',b),policy,b'STYLEOS S794 PASS',b''])

ok,reason=verify(fixture(),33);assert ok,reason
counter=0

def bad(raw,rc=33):
    global counter
    ok,_=verify(raw,rc)
    assert not ok,(rc,raw)
    counter+=1

bad(fixture(),32);bad(fixture(),0);bad(fixture(),None)
bad(fixture().replace(b'STYLEOS S794 PASS',b'STYLEOS S794 FAIL'))
bad(fixture()+b'STYLEOS S794 PASS\n')
bad(fixture().replace(POLICY,b'STYLEOS S794 POLICY production-cr3=allow'))
bad(fixture().replace(b'ExitBootServices OK',b'ExitBootServices FAIL'))
bad(fixture().replace(b'STYLEOS S794 SCAN',b'STYLEOS S794 SC4N'))
bad(fixture().replace(b'STYLEOS S794 BAR',b'STYLEOS S794 GAP'))
for which,alter in [(g,{'base':0x80001000}), (g,{'bytes':0}),
                    (s,{'match':0}), (s,{'display':0}),
                    (p,{'class':2}), (p,{'device':0x20}),
                    (p,{'vendor':0xffff}), (b,{'base':0x80001000}),
                    (b,{'width':64}), (b,{'raw-lo':0x80000009}),
                    (b,{'prefetch':0}), (b,{'index':7})]:
    gg,ss,pp,bb=[dict(x) for x in (g,s,p,b)]
    if which is g:gg.update(alter)
    if which is s:ss.update(alter)
    if which is p:pp.update(alter)
    if which is b:bb.update(alter)
    bad(fixture(gg,ss,pp,bb))
print(f'STYLEOS S794 NEGATIVE PASS {counter} corrupted fixtures rejected')
