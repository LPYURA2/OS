#!/usr/bin/env python3
"""S804 strict parser negatives; synthetic data never counted as hardware evidence."""
from cache_804_verify import FIELDS,POLICY,BASES,LENGTHS,verify

def line(kind,row):
    return b'STYLEOS S804 '+kind.encode()+b' '+b' '.join(
        k.encode()+b'=0x'+format(row[k],'016x').encode() for k in FIELDS[kind])
cpu={'cr0':0x80010033,'cr3':0x7801000,'pat':0x0007040600070406,
     'mtrrcap':0x508,'mtrrdef':0xc06,'physbits':40,'variable-count':8}
pci={'id':0x10431af4,'gop':BASES[0],'bar1raw':BASES[1],
     'bar4raw':0xc,'bar5raw':0xc0}
regs=[{'index':i,'base':0,'mask':0x800 if i==0 else 0} for i in range(8)]
parts=[b'STYLEOS x86_64 ExitBootServices OK',line('CPU',cpu),line('PCI',pci)]
parts += [line('MTRR',x) for x in regs]
for i,(base,length) in enumerate(zip(BASES,LENGTHS)):
    parts.append(line('RANGE',{'index':i,'base':base,'bytes':length,'end':base+length,'pages':length//4096}))
    cur=base;seg=0
    while cur<base+length:
        size=0x200000;stop=min(base+length,(cur&~(size-1))+size)
        parts.append(line('SEG',{'index':i,'segment':seg,'start':cur,'end':stop,
          'last-physical':stop-1,'page-size':size,'entry':(cur&~(size-1))|0x83,
          'pat-index':0,'pat-type':6,'mtrr-type':0,'pages':(stop-cur)//4096}))
        cur=stop;seg+=1
parts.extend([POLICY,b'STYLEOS S804 PASS'])
valid=b'\n'.join(parts)+b'\n'
assert verify(valid,33)[0],verify(valid,33)
def change(needle,value):
    assert needle in valid,(needle,value)
    return valid.replace(needle,value,1)
variants=[
 ('exit-0',valid,0),('double-PASS',valid+b'STYLEOS S804 PASS\n',33),
 ('missing-handoff',change(b'STYLEOS x86_64 ExitBootServices OK',b'broken'),33),
 ('fake-global-alias',change(b'global-alias-scan=unverified',b'global-alias-scan=verified'),33),
 ('fake-prod-cr3',change(b'production-cr3=deny',b'production-cr3=allow'),33),
 ('range-size',change(b'bytes=0x00000000003e8000',b'bytes=0x00000000003e7000'),33),
 ('range-end',change(b'end=0x00000000803e8000',b'end=0x00000000803e9000'),33),
 ('segment-gap',change(b'start=0x0000000080200000',b'start=0x0000000080201000'),33),
 ('segment-overlap',change(b'end=0x0000000080200000',b'end=0x0000000080201000'),33),
 ('wrong-physical',change(b'last-physical=0x00000000801fffff',b'last-physical=0x00000000801ffffe'),33),
 ('wrong-PAT',change(b'pat-type=0x0000000000000006',b'pat-type=0x0000000000000000'),33),
 ('wrong-MTRR',change(b'mtrr-type=0x0000000000000000',b'mtrr-type=0x0000000000000006'),33),
 ('wrong-PTE',change(b'entry=0x0000000080000083',b'entry=0x0000000080000003'),33),
 ('wrong-pages',change(b'pages=0x0000000000000200',b'pages=0x00000000000001ff'),33),
 ('wrong-PCI',change(b'bar1raw=0x0000000081000000',b'bar1raw=0x0000000081001000'),33),
 ('missing-seg',valid.replace(parts[-4]+b'\n',b''),33),
 ('double-seg',valid.replace(parts[-4]+b'\n',(parts[-4]+b'\n')*2),33),
 ('extra-marker',valid.replace(POLICY,b'STYLEOS S804 INJECT extra\n'+POLICY),33),
 ('missing-pass',valid.replace(b'STYLEOS S804 PASS\n',b''),33),
 ('wrong-pat-index',change(b'pat-index=0x0000000000000000',b'pat-index=0x0000000000000001'),33),
]
for name,data,rc in variants:
    if verify(data,rc)[0]:raise AssertionError('unsafe acceptance: '+name)
print('STYLEOS 804 negative UART cases PASS',len(variants))
