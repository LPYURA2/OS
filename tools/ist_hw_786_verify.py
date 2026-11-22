"""Verify independent QEMU exit and self-consistent live CPU IST transcript.
Never synthesizes evidence; standalone parser use does not imply hardware PASS.
"""
import re

HEX=rb'(0x[0-9a-f]{16})'
ALIAS=0x0000400000003000
PAGE=4096

def verify(uart: bytes, returncode: int | None) -> tuple[bool,str]:
    if returncode != 33:
        return False, f'QEMU debug-exit 33 required, observed {returncode}'
    if not isinstance(uart,bytes):return False,'bytes expected'
    lines=uart.replace(b'\r',b'').splitlines()
    forbidden=(b'STYLEOS S786 FAIL',b'STYLEOS PAGING ERROR',
        b'STYLEOS EMERGENCY',b'STYLEOS EXCEPTION',b'STYLEOS BOOT_ERROR',
        b'STYLEOS KERNEL HALT',b'firmware-return=1',b'guard-fault-injected=1')
    if any(x in uart for x in forbidden):return False,'failure marker present'
    def get(prefix,regex):
        found=[x for x in lines if x.startswith(prefix)]
        return re.fullmatch(regex,found[0]) if len(found)==1 else None
    ready=get(b'STYLEOS S786 READY ',
        rb'STYLEOS S786 READY old='+HEX+rb' new='+HEX+
        rb' alias='+HEX+rb' guard-lo='+HEX+rb' guard-hi='+HEX+
        rb' frame='+HEX+rb' cs='+HEX+rb' selector='+HEX+
        rb' idt='+HEX+rb' irq=0 nxe=1 own=9 tables=inactive-preflight')
    trap=get(b'STYLEOS S786 TRAP ',
        rb'STYLEOS S786 TRAP old='+HEX+rb' new='+HEX+
        rb' frame-rsp='+HEX+rb' saved-rip='+HEX+
        rb' expected-rip='+HEX+rb' saved-cs='+HEX+
        rb' tr='+HEX+rb' ist-top='+HEX+rb' leaf='+HEX+
        rb' guard-lo=0 guard-hi=0 pt-count=1 cr3=1 marker=1 hardware-int3=1')
    final=b'STYLEOS S786 TERMINAL qemu-exit=0x10 firmware-return=0 guard-fault-injected=0'
    if not ready or not trap or lines.count(final)!=1:
        return False,'missing or duplicated hardware marker'
    A=[int(x,16) for x in ready.groups()]
    B=[int(x,16) for x in trap.groups()]
    # ready: old,new,alias,guard_lo,guard_hi,frame,cs,selector,idt
    # trap:  old,new,frame_rsp,rip,expected_rip,cs,tr,ist_top,leaf
    if (A[0]==0 or A[1]==0 or A[0]==A[1] or
        (A[0] & 4095) or (A[1]&4095) or
        A[2]!=ALIAS or A[3]!=ALIAS-PAGE or A[4]!=ALIAS+PAGE or
        A[5]==0 or A[5]&4095 or not (0<A[5]<(1<<47)) or
        not A[6] or A[6]&7 or not A[7] or A[7]&7 or
        A[8]&4095 or A[8]==0 or
        B[0]!=A[0] or B[1]!=A[1] or
        not(ALIAS<=B[2]<=ALIAS+PAGE-5*8) or B[2]&7 or
        B[3]!=B[4] or B[3]==0 or B[3]>=(1<<47) or
        B[5]!=A[6] or B[6]!=A[7] or B[7]!=ALIAS+PAGE-16 or
        B[8]!=(A[5] | 0x8000000000000003)):
        return False,'live CR3/TR/IST frame/guard/CS mismatch'
    order=[(i,l.split()[2]) for i,l in enumerate(lines) if l.startswith(b'STYLEOS S786 ')]
    if [x for _,x in order]!=[b'READY',b'TRAP',b'TERMINAL'] or order[-1][0]!=len(lines)-1:
        return False,'wrong hardware marker order or unexpected tail'
    return True,'real terminal CR3+LTR+IST1 INT3 frame and absent active guard PTEs confirmed'
