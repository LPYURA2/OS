#!/usr/bin/env python3
"""Synthetic tamper cases: prove that a forged UART or normal QEMU exit fails."""
from ist_hw_786_verify import verify

def h(n):return f'0x{n:016x}'
old,new,alias,frame,cs,tr,idt=0x7801000,0x5f95000,0x400000003000,0x5f9e000,0x38,0x48,0x5fa2000
ready=(f'STYLEOS S786 READY old={h(old)} new={h(new)} alias={h(alias)} '
       f'guard-lo={h(alias-4096)} guard-hi={h(alias+4096)} frame={h(frame)} '
       f'cs={h(cs)} selector={h(tr)} idt={h(idt)} irq=0 nxe=1 own=9 tables=inactive-preflight')
trap=(f'STYLEOS S786 TRAP old={h(old)} new={h(new)} frame-rsp={h(alias+0xfb0)} '
      f'saved-rip={h(0x5f61234)} expected-rip={h(0x5f61234)} '
      f'saved-cs={h(cs)} tr={h(tr)} ist-top={h(alias+4080)} '
      f'leaf={h(frame|0x8000000000000063)} '
      'guard-lo=0 guard-hi=0 pt-count=1 cr3=1 marker=1 hardware-int3=1')
end='STYLEOS S786 TERMINAL qemu-exit=0x10 firmware-return=0 guard-fault-injected=0'
uart=('\r\n'.join([ready,trap,end])+'\r\n').encode()
assert verify(uart,33)[0],verify(uart,33)
for src,dst in [
    (' new='+h(new),' new='+h(old)),
    (' guard-lo=0',' guard-lo=1'),
    (' guard-hi=0',' guard-hi=1'),
    (' pt-count=1',' pt-count=0'),
    (' marker=1',' marker=0'),
    (' hardware-int3=1',' hardware-int3=0'),
    (' saved-rip='+h(0x5f61234),' saved-rip='+h(0x5f61235)),
    (' tr='+h(tr),' tr='+h(0)),
    (' leaf='+h(frame|0x8000000000000063),' leaf='+h(frame|3)),
    (' leaf='+h(frame|0x8000000000000063),' leaf='+h(frame|0x8000000000000023)),
    (' nxe=1',' nxe=0'),
    (' qemu-exit=0x10',' qemu-exit=0x11'),
    (' guard-fault-injected=0',' guard-fault-injected=1'),
    (' firmware-return=0',' firmware-return=1'),
    ('STYLEOS S786 TRAP','STYLEOS S786 TRAPX'),
]:
    broken=uart.replace(src.encode(),dst.encode(),1)
    assert not verify(broken,33)[0],(src,dst)
assert not verify(uart,0)[0]
assert not verify(uart,None)[0]
assert not verify(uart+uart.splitlines()[-1]+b'\n',33)[0]
assert not verify(uart.replace(b'\r\n',b'\n',1)+b'garbage\n',33)[0]
print('STYLEOS 786 SYNTHETIC NEGATIVE PARSER PASS')
