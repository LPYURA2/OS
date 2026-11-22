#!/usr/bin/env python3
"""Synthetic parser tests only: these never count as hardware QEMU evidence."""
import re
from tss_ltr_784_verify import verify

def h(n):return f'0x{n:016x}'
# Compact, explicitly synthetic transcript using the same expected schema.
base=0x5f55000
cr3=0x7801000
gdtorig=0x75dc000
gdt=base+0x8f000
tss=gdt+0x1000
rsp=tss+0x1000
ist=rsp+0x1000
code=0x38
idt=0x5fa2000
lines=[
 f'STYLEOS MEMMAP IMAGE {h(base)} {h(base+0xa6000)}',
 f'STYLEOS PAGING X86 GDT va={h(gdtorig)} pa={h(gdtorig)} bytes={h(0x48)} cs={h(code)} descriptor={h(0x00af9a000000ffff)}',
 f'STYLEOS PAGING X86 RETURN before={h(cr3)} candidate={h(base+0x1e000)} after={h(cr3)} cr0={h(0x80010033)} cr4={h(0x668)} efer={h(0xd00)}',
 f'STYLEOS DSI 781 INHERITED cr3={h(cr3)} cs={h(code)} tr={h(0)} gdt-limit={h(0x47)} idt-limit={h(0xfff)} gates={h(256)} ist-gates={h(0)} ist-mask={h(0)} boot-rsp={h(base+0xa1000)} boot-rsp-in-range=1 tss=unverified ist=unverified active=0 no-cr3-switch=1',
 *[f'STYLEOS DSI 782 FRAME {name} va={h(addr)} pa={h(addr)} bytes={h(4096)}' for name,addr in [('GDT',gdt),('TSS',tss),('RSP0',rsp),('IST1',ist)]],
 f'STYLEOS TGD 783 DESCRIPTOR cr3={h(cr3)} gdt-origin={h(gdtorig)} old-limit={h(0x47)} gdt-target={h(gdt)} tss-base={h(tss)} cs={h(code)} tss-selector={h(0x48)} new-limit={h(0x57)} rsp0-top={h(rsp+4080)} ist1-top={h(ist+4080)} valid=1 tr=0 lgdt=0 ltr=0 guard=unproven active=0 no-cr3-switch=1',
 f'STYLEOS TLT 784 READY cr3={h(cr3)} cs={h(code)} tr-before={h(0)} old-gdt={h(gdtorig)} new-gdt={h(gdt)} old-limit={h(0x47)} new-limit={h(0x57)} idt-base={h(idt)} selector={h(0x48)} tss={h(tss)} idt=own-256-ist0 if=0 pages=attested guard=unproven',
 f'STYLEOS TLT 784 VERIFIED cr3={h(cr3)} cs={h(code)} str={h(0x48)} gdt={h(gdt)} limit={h(0x57)} tss={h(tss)} busy={h(0x8b)} idt-base={h(idt)} lgdt=1 ltr=1 ist-delivery=untested guard=unproven',
 'STYLEOS TLT 784 TERMINAL qemu-exit=0x10 firmware-return=0 tr-restored=0'
]
raw=('\r\n'.join(lines)+'\r\n').encode()
assert verify(raw,33)[0],verify(raw,33)
for broken in [
    raw.replace(b' busy='+h(0x8b).encode(),b' busy='+h(0x89).encode()),
    raw.replace(b' str='+h(0x48).encode(),b' str='+h(0).encode()),
    raw.replace(b' lgdt=1',b' lgdt=0'),
    raw.replace(b' ist-delivery=untested',b' ist-delivery=verified'),
    raw.replace(b'guard=unproven',b'guard=verified'),
    raw.replace(b' tr-before='+h(0).encode(),b' tr-before='+h(0x48).encode()),
    raw.replace(b' pages=attested',b' pages=unproven'),
    raw.replace(b' firmware-return=0',b' firmware-return=1'),
    raw.replace(b' qemu-exit=0x10',b' qemu-exit=0x11'),
    raw.replace(b'STYLEOS TLT 784 VERIFIED',b'STYLEOS TLT 784 VERTIFIED'),
    raw+raw.split(b'\r\n')[-2]+b'\r\n',
]:
    assert not verify(broken,33)[0],broken[-120:]
assert not verify(raw,0)[0]
assert not verify(raw,None)[0]
print('STYLEOS 784 SYNTHETIC PARSER TAMPER REFUSALS PASS')
