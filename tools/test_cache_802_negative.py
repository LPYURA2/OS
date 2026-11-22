#!/usr/bin/env python3
"""Counterexamples for 802. Synthetic fixtures do NOT prove real hardware."""
from cache_800_verify import KEYS as K800,POLICY as P800,verify as v800
from cache_801_verify import KEYS as K801,POLICY as P801,verify as v801
from cache_802_verify import reconcile

def rec(stage,keys,kind,**entries):
    return (f'STYLEOS S{stage} {kind} '+ ' '.join(f'{k}=0x{entries[k]:016x}' for k in keys[kind])).encode()
pat=0x0007040600070406
mtrrs=[rec(800,K800,'MTRR',index=i,base=([0x80000000,0xc000000000]+[0]*6)[i],mask=([0xff80000800,0xe000000800]+[0]*6)[i]) for i in range(8)]
a=[b'STYLEOS x86_64 ExitBootServices OK',
rec(800,K800,'CPU',cr0=0x80010033,cr3=0x7801000,pat=pat,mtrrcap=0x508,mtrrdef=0xc06,physbits=40,**{'variable-count':8}),
rec(800,K800,'PCI',id=0x10431af4,gop=0x80000000,bar1raw=0x81000000,bar4raw=0xc,bar5raw=0xc0),*mtrrs]
for i,x in enumerate((0x80000000,0x81000000,0xc000000000)):
 a.append(rec(800,K800,'TARGET',index=i,address=x,**{'mtrr-type':0,'matches':1,'enabled':1}))
a.extend([P800,b'STYLEOS S800 PASS']); uart800=b'\n'.join(a)+b'\n'
b=[b'STYLEOS x86_64 ExitBootServices OK',rec(801,K801,'CPU',cr0=0x80010033,cr3=0x7801000,pat=pat,physbits=40)]
for i,x in enumerate((0x80000000,0x81000000,0xc000000000)):
 b.append(rec(801,K801,'TARGET',index=i,address=x,present=1,translated=x,**{'page-size':2097152,'entry':x|0x83,'pat-index':0,'pat-type':6}))
b.extend([P801,b'STYLEOS S801 PASS']);uart801=b'\n'.join(b)+b'\n'
assert v800(uart800,33)[0],v800(uart800,33)
assert v801(uart801,33)[0],v801(uart801,33)
assert reconcile(uart800,uart801,33,33)[0],reconcile(uart800,uart801,33,33)

def change(raw,old,new):
    assert old in raw,(old,raw)
    return raw.replace(old,new,1)

# Cases validated by S800/S801 individually but rejected by the cross-boot policy.
others=[
 (uart800,change(uart801,b'cr3=0x0000000007801000',b'cr3=0x0000000007802000'),33,33),
 (uart800,change(uart801,b'pat=0x0007040600070406',b'pat=0x0007040600070006'),33,33),
 (change(uart800,b'pat=0x0007040600070406',b'pat=0x0007040600070006'),uart801,33,33),
 (change(uart800,b'cr3=0x0000000007801000',b'cr3=0x0000000007802000'),uart801,33,33),
]
# Make a self-consistent (but non-identity) PTE sample: physical output differs.
old=b'translated=0x0000000080000000'
other=change(uart801,old,b'translated=0x0000000080200000')
other=change(other,b'entry=0x0000000080000083',b'entry=0x0000000080200083')
others.append((uart800,other,33,33))
# Change MTRR result to WB coherently by disabling variable matches at all 3 addresses.
# First example is enough to prove that a valid UART cannot force a joined UC verdict.
other=change(uart800,b'base=0x0000000080000000 mask=0x000000ff80000800',b'base=0x0000000080000000 mask=0x0000000000000000')
other=change(other,b'mtrr-type=0x0000000000000000 matches=0x0000000000000001 enabled=0x0000000000000001',b'mtrr-type=0x0000000000000006 matches=0x0000000000000000 enabled=0x0000000000000001')
others.append((other,uart801,33,33))
# Wrong exit, absent required sample, or forged policy are rejected as well.
others.extend([(uart800,uart801,32,33),(uart800,uart801,33,32),
(change(uart800,b'production-cr3=deny',b'production-cr3=allow'),uart801,33,33),
(uart800,change(uart801,b'global-alias-scan=unverified',b'global-alias-scan=verified'),33,33)])
for index,(x,y,rcx,rcy) in enumerate(others):
 valid,why=reconcile(x,y,rcx,rcy)
 assert not valid,(index,why)
# Demonstrate that we really exercised cross-boot checks, not just S800/S801 parsers.
assert v800(others[0][0],33)[0] and v801(others[0][1],33)[0]
assert v800(others[4][0],33)[0] and v801(others[4][1],33)[0]
print(f'STYLEOS S802 negative evidence PASS {len(others)} rejected (includes cross-boot valid traces)')
