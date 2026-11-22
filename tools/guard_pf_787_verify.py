"""Fail-closed parser for authentic stage-787 QEMU UART/exit evidence.

A synthetic transcript MAY be tested negatively; it cannot replace QEMU proof.
"""
import re

HEX=rb'(0x[0-9a-f]{16})'
IST1=0x0000400000003000
IST2=IST1+3*4096
GUARD=IST1-4096
PAGE=4096

def verify(uart:bytes,returncode:int|None)->tuple[bool,str]:
    if returncode!=33:return False,f'exit 33 required, actual {returncode}'
    if not isinstance(uart,bytes):return False,'UART must be bytes'
    lines=uart.replace(b'\r',b'').splitlines()
    if any(x in uart for x in (b'STYLEOS S787 FAIL',b'STYLEOS PAGING ERROR',
        b'STYLEOS EMERGENCY',b'STYLEOS EXCEPTION',b'STYLEOS BOOT_ERROR',
        b'STYLEOS KERNEL HALT',b'firmware-return=1')):
        return False,'fatal marker observed'
    def match(prefix,pattern):
        found=[line for line in lines if line.startswith(prefix)]
        return re.fullmatch(pattern,found[0]) if len(found)==1 else None
    ready=match(b'STYLEOS S787 READY ',
        rb'STYLEOS S787 READY old='+HEX+rb' new='+HEX+
        rb' alias='+HEX+rb' guard-lo='+HEX+rb' guard-hi='+HEX+
        rb' frame='+HEX+rb' cs='+HEX+rb' selector='+HEX+
        rb' idt='+HEX+rb' ist2='+HEX+rb' guard='+HEX+rb' frame2='+HEX+
        rb' irq=0 nxe=1 own=10 tables=inactive-preflight')
    bp=match(b'STYLEOS S787 BP ',
        rb'STYLEOS S787 BP cr3='+HEX+rb' rsp='+HEX+
        rb' saved-rip='+HEX+rb' expected-rip='+HEX+rb' tr='+HEX)
    pf=match(b'STYLEOS S787 PF ',
        rb'STYLEOS S787 PF cr2='+HEX+rb' err='+HEX+rb' rsp='+HEX+
        rb' saved-rip='+HEX+rb' expected-rip='+HEX+
        rb' old-rsp='+HEX+rb' cs='+HEX+rb' tr='+HEX+
        rb' leaf1='+HEX+rb' leaf2='+HEX+
        rb' guard=0 hardware-pf=1 ist2=1')
    terminal=b'STYLEOS S787 TERMINAL qemu-exit=0x10 firmware-return=0 guard-page-was-present=0 stack-overflow-recovery=0'
    if not ready or not bp or not pf or lines.count(terminal)!=1:
        return False,'missing/duplicated/malformed attested records'
    a=[int(x,16) for x in ready.groups()]
    b=[int(x,16) for x in bp.groups()]
    c=[int(x,16) for x in pf.groups()]
    # ready: 0old 1new 2alias 3guardlo 4guardhi 5frame1 6cs 7tr 8idt 9ist2 10guard 11frame2
    # BP: 0cr3 1rsp 2rip 3expected 4tr
    # PF: 0cr2 1err 2rsp 3rip 4expected 5oldrsp 6cs 7tr 8leaf1 9leaf2
    if (not a[0] or not a[1] or a[0]==a[1] or (a[0]|a[1])&4095 or
        a[2]!=IST1 or a[3]!=GUARD or a[4]!=IST1+PAGE or
        a[9]!=IST2 or a[10]!=GUARD or
        not a[5] or not a[11] or a[5]==a[11] or
        (a[5]|a[11])&4095 or a[5]>=(1<<47) or a[11]>=(1<<47) or
        not a[6] or a[6]&7 or not a[7] or a[7]&7 or
        not a[8] or a[8]&4095 or
        b[0]!=a[1] or not(IST1<=b[1]<=IST1+PAGE-40) or b[1]&7 or
        b[2]!=b[3] or not b[2] or b[2]>=(1<<47) or b[4]!=a[7] or
        c[0]!=GUARD or c[1]!=2 or
        not(IST2<=c[2]<=IST2+PAGE-48) or c[2]&7 or
        c[3]!=c[4] or not c[3] or c[3]>=(1<<47) or
        not (IST1<=c[5]<IST1+PAGE) or
        c[6]!=a[6] or c[7]!=a[7] or
        c[8]!=(a[5]|0x8000000000000063) or
        c[9]!=(a[11]|0x8000000000000063)):
        return False,'hardware #PF CR2/error/RIP/stack/root consistency mismatch'
    entries=[(i,line.split()[2]) for i,line in enumerate(lines) if line.startswith(b'STYLEOS S787 ')]
    if [x for _,x in entries]!=[b'READY',b'BP',b'PF',b'TERMINAL'] or entries[-1][0]!=len(lines)-1:
        return False,'hardware records out of order or trailing output'
    return True,'hardware INT3 on IST1, unmapped guard write and page fault error=2 via IST2 verified'
