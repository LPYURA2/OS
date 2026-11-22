"""Independent, tamper-sensitive UART + QEMU exit verifier for terminal 784.

This does not generate hardware evidence. Only real UART from a QEMU process
that exited via the independent ISA debug-exit device is acceptable.
"""
import re

HEX = rb'(0x[0-9a-f]{16})'

def one(lines, pattern):
    found=[re.fullmatch(pattern,line) for line in lines if line.startswith(pattern.split(b' ')[0]+b' ')]
    matches=[m for m in found if m is not None]
    return matches[0] if len(matches)==1 and len(found)==1 else None

def verify(uart: bytes, returncode: int) -> tuple[bool,str]:
    if returncode != 33:
        return False,f'QEMU debug-exit code 33 required, observed {returncode}'
    if not isinstance(uart,bytes):return False,'bytes expected'
    lines=uart.replace(b'\r',b'').splitlines()
    banned=(b'STYLEOS TLT 784 FAIL',b'STYLEOS PAGING ERROR',
            b'STYLEOS EMERGENCY',b'STYLEOS EXCEPTION',b'STYLEOS BOOT_ERROR',
            b'STYLEOS KERNEL HALT',b'STYLEOS PAGING ROUNDTRIP PASS',
            b'firmware-return=1',b'guard=verified',b'ist-delivery=verified')
    if any(x in uart for x in banned):return False,'fault, firmware path, or unsupported claim'
    if lines.count(b'STYLEOS TLT 784 TERMINAL qemu-exit=0x10 firmware-return=0 tr-restored=0')!=1:
        return False,'terminal signature is missing or repeated'
    def get(marker,pattern):
        found=[x for x in lines if x.startswith(marker)]
        return re.fullmatch(pattern,found[0]) if len(found)==1 else None
    ready=get(b'STYLEOS TLT 784 READY ',
        rb'STYLEOS TLT 784 READY cr3='+HEX+rb' cs='+HEX+
        rb' tr-before='+HEX+rb' old-gdt='+HEX+rb' new-gdt='+HEX+
        rb' old-limit='+HEX+rb' new-limit='+HEX+rb' idt-base='+HEX+
        rb' selector='+HEX+rb' tss='+HEX+
        rb' idt=own-256-ist0 if=0 pages=attested guard=unproven')
    observed=get(b'STYLEOS TLT 784 VERIFIED ',
        rb'STYLEOS TLT 784 VERIFIED cr3='+HEX+rb' cs='+HEX+
        rb' str='+HEX+rb' gdt='+HEX+rb' limit='+HEX+rb' tss='+HEX+
        rb' busy='+HEX+rb' idt-base='+HEX+
        rb' lgdt=1 ltr=1 ist-delivery=untested guard=unproven')
    staged=get(b'STYLEOS TGD 783 DESCRIPTOR ',
        rb'STYLEOS TGD 783 DESCRIPTOR cr3='+HEX+
        rb' gdt-origin='+HEX+rb' old-limit='+HEX+
        rb' gdt-target='+HEX+rb' tss-base='+HEX+rb' cs='+HEX+
        rb' tss-selector='+HEX+rb' new-limit='+HEX+
        rb' rsp0-top='+HEX+rb' ist1-top='+HEX+
        rb' valid=1 tr=0 lgdt=0 ltr=0 guard=unproven active=0 no-cr3-switch=1')
    old=get(b'STYLEOS PAGING X86 GDT ',
        rb'STYLEOS PAGING X86 GDT va='+HEX+rb' pa='+HEX+
        rb' bytes='+HEX+rb' cs='+HEX+rb' descriptor='+HEX)
    inherited=get(b'STYLEOS DSI 781 INHERITED ',
        rb'STYLEOS DSI 781 INHERITED cr3='+HEX+rb' cs='+HEX+
        rb' tr='+HEX+rb' gdt-limit='+HEX+rb' idt-limit='+HEX+
        rb' gates='+HEX+rb' ist-gates='+HEX+rb' ist-mask='+HEX+
        rb' boot-rsp='+HEX+
        rb' boot-rsp-in-range=1 tss=unverified ist=unverified active=0 no-cr3-switch=1')
    ret=get(
        b'STYLEOS PAGING X86 RETURN ',
        rb'STYLEOS PAGING X86 RETURN before='+HEX+rb' candidate='+HEX+rb' after='+HEX+
        rb' cr0='+HEX+rb' cr4='+HEX+rb' efer='+HEX)
    if not all((ready,observed,staged,old,inherited,ret)):
        return False,'missing/duplicated prior 781/783 or new 784 hardware record'
    R=[int(x,16) for x in ready.groups()]
    V=[int(x,16) for x in observed.groups()]
    S=[int(x,16) for x in staged.groups()]
    O=[int(x,16) for x in old.groups()]
    I=[int(x,16) for x in inherited.groups()]
    T=[int(x,16) for x in ret.groups()]
    # 783: cr3,gdt-origin,old-limit,gdt-target,tss-base,cs,selector,new-limit,rsp0,ist1
    # ready: cr3,cs,tr,old-gdt,new-gdt,old-limit,new-limit,idt,selector,tss
    # observed: cr3,cs,str,gdt,limit,tss,busy,idt
    if (R[0]!=S[0] or R[0]!=I[0] or R[0]!=T[0] or R[0]!=T[2] or
        R[1]!=S[5] or R[1]!=I[1] or R[1]!=O[3] or R[2]!=0 or I[2]!=0 or
        R[3]!=S[1] or R[3]!=O[0] or R[3]!=O[1] or
        R[4]!=S[3] or R[5]!=S[2] or R[6]!=S[7] or
        R[8]!=S[6] or R[9]!=S[4] or R[6]!=R[8]+15 or
        O[2]!=R[5]+1 or R[8]!=R[5]+1 or R[8]%8 or
        I[3]!=R[5] or I[4]!=4095 or I[5]!=256 or I[6]!=0 or I[7]!=0 or
        V[0]!=R[0] or V[1]!=R[1] or V[2]!=R[8] or
        V[3]!=R[4] or V[4]!=R[6] or V[5]!=R[9] or
        V[6]!=0x8b or V[7]!=R[7]):
        return False,'live STR/busy/GDTR/IDTR mismatch against 781-783 hardware evidence'
    frames={}
    for n in (b'GDT',b'TSS',b'RSP0',b'IST1'):
        m=get(b'STYLEOS DSI 782 FRAME '+n+b' ',
              rb'STYLEOS DSI 782 FRAME '+n+rb' va='+HEX+rb' pa='+HEX+rb' bytes='+HEX)
        if not m:return False,'782 individual mapped frame missing'
        a,pa,size=[int(x,16) for x in m.groups()]
        if a!=pa or size!=4096 or a%4096:return False,'782 frame non-identity'
        frames[n]=a
    img=get(b'STYLEOS MEMMAP IMAGE ',rb'STYLEOS MEMMAP IMAGE '+HEX+rb' '+HEX)
    if not img:return False,'UEFI IMAGE extent absent'
    start,end=[int(x,16) for x in img.groups()]
    if (len(set(frames.values()))!=4 or
        any(not(start<=x and x+4096<=end) for x in frames.values()) or
        frames[b'GDT']!=R[4] or frames[b'TSS']!=R[9] or
        S[8]!=frames[b'RSP0']+4080 or S[9]!=frames[b'IST1']+4080):
        return False,'783 descriptor not tied to individually mapped 782 frames'
    indexes=[i for i,line in enumerate(lines) if line.startswith(b'STYLEOS TLT 784 ')]
    names=[lines[i].split()[3] for i in indexes]
    if names!=[b'READY',b'VERIFIED',b'TERMINAL']:
        return False,'wrong terminal marker order or extra 784 markers'
    if not (indexes[0]<indexes[1]<indexes[2]==len(lines)-1):
        return False,'unexpected tail after terminal'
    return True,'real hardware LGDT/LTR/STR, busy bit and terminal debug-exit attested'
