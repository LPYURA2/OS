#!/usr/bin/env python3
"""Negative tamper tests for 787 parser; never evidence of running QEMU."""
from guard_pf_787_verify import verify
h=lambda x: f'0x{x:016x}'
base=0x400000003000; ist2=base+12288; guard=base-4096
old,new,f1,f2,cs,tr,idt=0x7801000,0x5f95000,0x5f9e000,0x5fa3000,0x38,0x48,0x5fa2000
ready=(f'STYLEOS S787 READY old={h(old)} new={h(new)} alias={h(base)} guard-lo={h(guard)} '
    f'guard-hi={h(base+4096)} frame={h(f1)} cs={h(cs)} selector={h(tr)} idt={h(idt)} '
    f'ist2={h(ist2)} guard={h(guard)} frame2={h(f2)} irq=0 nxe=1 own=10 tables=inactive-preflight')
bp=(f'STYLEOS S787 BP cr3={h(new)} rsp={h(base+0xfb0)} saved-rip={h(0x5f61234)} '
    f'expected-rip={h(0x5f61234)} tr={h(tr)}')
pf=(f'STYLEOS S787 PF cr2={h(guard)} err={h(2)} rsp={h(ist2+0xfa0)} '
    f'saved-rip={h(0x5f61298)} expected-rip={h(0x5f61298)} old-rsp={h(base+0xf88)} '
    f'cs={h(cs)} tr={h(tr)} leaf1={h(f1|0x8000000000000063)} '
    f'leaf2={h(f2|0x8000000000000063)} guard=0 hardware-pf=1 ist2=1')
end='STYLEOS S787 TERMINAL qemu-exit=0x10 firmware-return=0 guard-page-was-present=0 stack-overflow-recovery=0'
uart=('\r\n'.join((ready,bp,pf,end))+'\r\n').encode()
assert verify(uart,33)[0],verify(uart,33)
for oldpart,newpart in [
    (' new='+h(new),' new='+h(old)),(' ist2='+h(ist2),' ist2='+h(base)),
    (' frame2='+h(f2),' frame2='+h(f1)),(' cr3='+h(new),' cr3='+h(old)),
    (' guard='+h(guard),' guard='+h(guard+4096)),
    (' cr2='+h(guard),' cr2='+h(guard+4096)),
    (' err='+h(2),' err='+h(3)),(' err='+h(2),' err='+h(0)),
    (' saved-rip='+h(0x5f61298),' saved-rip='+h(0x5f61299)),
    (' old-rsp='+h(base+0xf88),' old-rsp='+h(ist2+0xf88)),
    (' leaf2='+h(f2|0x8000000000000063),' leaf2='+h(f2|0x8000000000000023)),
    (' tr='+h(tr),' tr='+h(0)),(' hardware-pf=1',' hardware-pf=0'),
    (' ist2=1',' ist2=0'),(' guard=0',' guard=1'),
    (' qemu-exit=0x10',' qemu-exit=0x11'),
    (' stack-overflow-recovery=0',' stack-overflow-recovery=1'),
    ('STYLEOS S787 BP','STYLEOS S787 BPX'),
]:
    broken=uart.replace(oldpart.encode(),newpart.encode(),1)
    assert broken!=uart
    assert not verify(broken,33)[0],(oldpart,newpart)
for data,exit_code in [(uart,0),(uart,None),(uart+uart,33),
    (uart+b'STYLEOS S787 FAIL counterfeit\n',33),
    (uart+b'extra marker\n',33)]:
    assert not verify(data,exit_code)[0]
print('STYLEOS 787 PARSER NEGATIVE PASS')
