#!/usr/bin/env python3
"""Negative tamper tests. A synthetic positive parser fixture is NOT hardware proof."""
from root_slot_789_verify import verify
h=lambda n:f'0x{n:016x}'
b=0x400000003000; i2=b+3*4096; i3=b+6*4096; g=b-4096
old,new,f1,f2,f3,cs,tr,idt=0x7801000,0x5f95000,0x5f9e000,0x5fa3000,0x5fa4000,0x38,0x48,0x5fa2000
ready=(f'STYLEOS S789 READY old={h(old)} new={h(new)} alias={h(b)} guard-lo={h(g)} '
 f'guard-hi={h(b+4096)} frame={h(f1)} cs={h(cs)} selector={h(tr)} idt={h(idt)} '
 f'ist2={h(i2)} guard={h(g)} frame2={h(f2)} ist3={h(i3)} frame3={h(f3)} root0={h(0x100003)} root128={h(0x770003)} '
 'inherited-root-slots=1 root-present-slots=2 irq=0 nxe=1 own=11 pf-gate-present=0 tables=inactive-preflight')
bp=(f'STYLEOS S789 BP cr3={h(new)} rsp={h(b+0xfb0)} saved-rip={h(0x5f61234)} '
 f'expected-rip={h(0x5f61234)} tr={h(tr)}')
df=(f'STYLEOS S789 DF cr2={h(g)} err={h(0)} rsp={h(i3+0xfa0)} '
 f'saved-rip={h(0x5f61298)} expected-rip={h(0x5f61298)} old-rsp={h(b+0xf88)} '
 f'cs={h(cs)} tr={h(tr)} leaf1={h(f1|0x8000000000000063)} '
 f'leaf2={h(f2|0x8000000000000003)} leaf3={h(f3|0x8000000000000063)} root0={h(0x100003)} root128={h(0x770023)} '
 'inherited-root-slots=1 root-present-slots=2 pf-gate-present=0 df-ist3=1 hardware-df=1')
end=('STYLEOS S789 TERMINAL qemu-exit=0x10 firmware-return=0 '
 'pf-handler-entered=0 df-hardware=1 pml4-pruned=1 independent-low-tables=0 stack-overflow-recovery=0')
uart=('\r\n'.join((ready,bp,df,end))+'\r\n').encode()
assert verify(uart,33)[0],verify(uart,33)
for before,after in [
 (' new='+h(new),' new='+h(old)),(' ist3='+h(i3),' ist3='+h(i2)),
 (' frame3='+h(f3),' frame3='+h(f2)),(' guard='+h(g),' guard='+h(b)),
 (' cr3='+h(new),' cr3='+h(old)),(' cr2='+h(g),' cr2='+h(b)),
 (' err='+h(0),' err='+h(2)),(' err='+h(0),' err='+h(1)),
 (' saved-rip='+h(0x5f61298),' saved-rip='+h(0x5f61299)),
 (' old-rsp='+h(b+0xf88),' old-rsp='+h(i3+0xf88)),
 (' leaf3='+h(f3|0x8000000000000063),' leaf3='+h(f3|0x8000000000000023)),
 (' tr='+h(tr),' tr='+h(0)),(' hardware-df=1',' hardware-df=0'),
 (' df-ist3=1',' df-ist3=0'),(' pf-gate-present=0',' pf-gate-present=1'),
 (' root0='+h(0x100003),' root0='+h(0x100000)),
 (' root128='+h(0x770003),' root128='+h(0x770000)),
 (' root128='+h(0x770023),' root128='+h(0x770003)),
 (' root-present-slots=2',' root-present-slots=3'),
 (' pml4-pruned=1',' pml4-pruned=0'),
 (' qemu-exit=0x10',' qemu-exit=0x11'),(' stack-overflow-recovery=0',' stack-overflow-recovery=1'),
 ('STYLEOS S789 DF','STYLEOS S789 PFX')]:
    corrupt=uart.replace(before.encode(),after.encode(),1)
    assert corrupt!=uart
    assert not verify(corrupt,33)[0],(before,after)
for data,code in [(uart,0),(uart,None),(uart+uart,33),
 (uart+b'STYLEOS S789 FAIL counterfeit\n',33),
 (uart+b'trailing junk\n',33)]:
    assert not verify(data,code)[0]
print('STYLEOS 789 PARSER NEGATIVE PASS')
