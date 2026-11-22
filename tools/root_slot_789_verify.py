"""Fail-closed check of a *real* stage-789 QEMU/OVMF UART and process exit.

Synthetic transcripts are allowed only as NEGATIVE parser tests, never evidence.
"""
import re

HEX=rb'(0x[0-9a-f]{16})'
IST1=0x0000400000003000
IST2=IST1+3*4096
IST3=IST1+6*4096
GUARD=IST1-4096
PAGE=4096


def verify(uart:bytes,returncode:int|None)->tuple[bool,str]:
    if returncode!=33: return False,f'exit 33 required; received {returncode}'
    if not isinstance(uart,bytes): return False,'UART must be raw bytes'
    lines=uart.replace(b'\r',b'').splitlines()
    if any(s in uart for s in (b'STYLEOS S789 FAIL',b'STYLEOS PAGING ERROR',
        b'STYLEOS EXCEPTION',b'STYLEOS EMERGENCY',b'STYLEOS BOOT_ERROR',
        b'STYLEOS KERNEL HALT',b'firmware-return=1')):
        return False,'fatal marker in UART'
    def exactly(prefix,pattern):
        matches=[line for line in lines if line.startswith(prefix)]
        return re.fullmatch(pattern,matches[0]) if len(matches)==1 else None
    ready=exactly(b'STYLEOS S789 READY ',
        rb'STYLEOS S789 READY old='+HEX+rb' new='+HEX+rb' alias='+HEX+
        rb' guard-lo='+HEX+rb' guard-hi='+HEX+rb' frame='+HEX+
        rb' cs='+HEX+rb' selector='+HEX+rb' idt='+HEX+
        rb' ist2='+HEX+rb' guard='+HEX+rb' frame2='+HEX+
        rb' ist3='+HEX+rb' frame3='+HEX+rb' root0='+HEX+rb' root128='+HEX+
        rb' inherited-root-slots=1 root-present-slots=2 irq=0 nxe=1 own=11 pf-gate-present=0 tables=inactive-preflight')
    bp=exactly(b'STYLEOS S789 BP ',
        rb'STYLEOS S789 BP cr3='+HEX+rb' rsp='+HEX+
        rb' saved-rip='+HEX+rb' expected-rip='+HEX+rb' tr='+HEX)
    df=exactly(b'STYLEOS S789 DF ',
        rb'STYLEOS S789 DF cr2='+HEX+rb' err='+HEX+rb' rsp='+HEX+
        rb' saved-rip='+HEX+rb' expected-rip='+HEX+rb' old-rsp='+HEX+
        rb' cs='+HEX+rb' tr='+HEX+rb' leaf1='+HEX+
        rb' leaf2='+HEX+rb' leaf3='+HEX+rb' root0='+HEX+rb' root128='+HEX+
        rb' inherited-root-slots=1 root-present-slots=2 pf-gate-present=0 df-ist3=1 hardware-df=1')
    end=(b'STYLEOS S789 TERMINAL qemu-exit=0x10 firmware-return=0 '
         b'pf-handler-entered=0 df-hardware=1 pml4-pruned=1 independent-low-tables=0 stack-overflow-recovery=0')
    if not ready or not bp or not df or lines.count(end)!=1:
        return False,'missing/duplicate/malformed mandatory records'
    a=[int(x,16) for x in ready.groups()]
    b=[int(x,16) for x in bp.groups()]
    d=[int(x,16) for x in df.groups()]
    # READY 0old 1new 2alias 3lowerGuard 4upperGuard 5frame1
    # 6cs 7tr 8idt 9ist2 10guard 11frame2 12ist3 13frame3
    # BP 0cr3 1rsp 2rip 3expected 4tr
    # DF 0cr2 1err 2rsp 3rip 4expected 5oldRsp 6cs 7tr 8leaf1 9leaf2 10leaf3
    if (not a[0] or not a[1] or a[0]==a[1] or (a[0]|a[1])&4095 or
        a[2]!=IST1 or a[3]!=GUARD or a[4]!=IST1+PAGE or
        a[9]!=IST2 or a[10]!=GUARD or a[12]!=IST3 or
        not a[5] or not a[11] or not a[13] or
        len({a[5],a[11],a[13]})!=3 or
        (a[5]|a[11]|a[13])&4095 or
        max(a[5],a[11],a[13])>=(1<<47) or
        not a[6] or a[6]&7 or not a[7] or a[7]&7 or
        not a[8] or a[8]&4095 or
        b[0]!=a[1] or not(IST1<=b[1]<=IST1+PAGE-40) or b[1]&7 or
        b[2]!=b[3] or not b[2] or b[2]>=(1<<47) or b[4]!=a[7] or
        d[0]!=GUARD or d[1]!=0 or
        not(IST3<=d[2]<=IST3+PAGE-48) or d[2]&7 or
        d[3]!=d[4] or not d[3] or d[3]>=(1<<47) or
        not(IST1<=d[5]<IST1+PAGE) or d[6]!=a[6] or d[7]!=a[7] or
        d[8]!=(a[5]|0x8000000000000063) or
        (d[9]&~0x60)!=(a[11]|0x8000000000000003) or
        d[10]!=(a[13]|0x8000000000000063) or
        a[14]!=d[11] or a[15]!=d[12] or
        (a[14]&1)==0 or (a[15]&3)!=3 or
        (a[14]&0x000ffffffffff000)==0 or
        (a[15]&0x000ffffffffff000)==0):
        return False,'#DF CR2/zero-error/hardware RIP/IST3/frame/leaf mismatch'
    order=[(i,line.split()[2]) for i,line in enumerate(lines)
           if line.startswith(b'STYLEOS S789 ')]
    if [v for _,v in order]!=[b'READY',b'BP',b'DF',b'TERMINAL'] or order[-1][0]!=len(lines)-1:
        return False,'records out of order, duplicated or trailing output'
    return True,'hardware nested PF delivery failure produced #DF with zero error on IST3'
