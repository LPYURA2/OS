"""Verify two independent real QEMU x86 hardware #PF policies; synthetic inputs are never evidence."""
import re
HEX = rb'0x[0-9a-f]{16}'
PAGE=4096
IST1=0x0000400000003000
IST2=IST1+3*PAGE
IST3=IST1+6*PAGE

KEYS={
  'READY': ('mode old new alias guard-lo guard-hi frame cs selector idt ist2 guard frame2 ist3 frame3 root0 root128 oldroot0 low-table low-used code-rx data-rw-nx ro-nx stack-rw-nx rx-target nx-target').split(),
  'BP': ('cr3 rsp saved-rip expected-rip tr').split(),
  'PF': ('mode cr2 err rsp rip expected-rip target old-rsp cr3 tr cr0 efer leaf-flags low-table root0 root128').split(),
}
TRAILERS={
 'READY': b' inherited-root-slots=0 root-present-slots=2 irq=0 nxe=1 own=11 pf-gate-present=1 tables=inactive-preflight',
 'BP': b'',
 'PF': b' pf-present=1 pf-ist2=1 firmware-return=0',
}
END=b'STYLEOS S791 TERMINAL qemu-exit=0x10 hardware-pf=1 cr0-wp=1 efer-nxe=1 owned-low=1 code-write-allowed=0 nx-execution-allowed=0'

def parse(line:bytes,record:str):
    keys=KEYS[record]
    prefix=b'STYLEOS S791 '+record.encode()+b' '
    pattern=prefix+b' '.join(re.escape(k.encode())+b'='+b'('+HEX+b')' for k in keys)+TRAILERS[record]
    m=re.fullmatch(pattern,line)
    return {k:int(v,16) for k,v in zip(keys,m.groups())} if m else None

def verify(uart:bytes,rc:int|None,mode:int)->tuple[bool,str]:
    if mode not in (1,2): return False,'invalid experiment mode'
    if rc!=33: return False,'QEMU debug-exit code 33 required'
    if not isinstance(uart,bytes): return False,'UART must be raw'
    if any(t in uart for t in (b'STYLEOS S791 FAIL',b'STYLEOS EMERGENCY',b'STYLEOS BOOT_ERROR',b'firmware-return=1')):
        return False,'fatal marker'
    lines=uart.replace(b'\r',b'').splitlines()
    records=[]
    for rec in ['READY','BP','PF']:
        matches=[line for line in lines if line.startswith(b'STYLEOS S791 '+rec.encode()+b' ')]
        if len(matches)!=1:return False,'missing/duplicate '+rec
        data=parse(matches[0],rec)
        if data is None:return False,'malformed '+rec
        records.append(data)
    if lines.count(END)!=1 or not lines or lines[-1]!=END:
        return False,'terminal record missing/extra output'
    if [l.split()[2] for l in lines if l.startswith(b'STYLEOS S791 ')]!=[b'READY',b'BP',b'PF',b'TERMINAL']:
        return False,'record order/duplication mismatch'
    a,b,p=records
    test_target=a['rx-target'] if mode==1 else a['nx-target']
    expected_pf_code=3 if mode==1 else 17
    if (a['mode']!=mode or p['mode']!=mode or
       not a['old'] or not a['new'] or a['old']==a['new'] or
       (a['old']|a['new'])&(PAGE-1) or
       a['alias']!=IST1 or a['guard-lo']!=IST1-PAGE or a['guard-hi']!=IST1+PAGE or
       a['ist2']!=IST2 or a['ist3']!=IST3 or a['guard']!=IST1-PAGE or
       not a['frame'] or not a['frame2'] or not a['frame3'] or
       len({a['frame'],a['frame2'],a['frame3']})!=3 or
       (a['frame']|a['frame2']|a['frame3'])&(PAGE-1) or
       not a['cs'] or a['cs']&7 or not a['selector'] or a['selector']&7 or
       not a['idt'] or a['idt']&(PAGE-1) or
       a['low-table']&(PAGE-1) or a['low-table']==0 or
       not 4<=a['low-used']<=32 or
       not 1<=a['code-rx']<=8192 or not 1<=a['data-rw-nx']<=8192 or
       not 1<=a['ro-nx']<=8192 or not 1<=a['stack-rw-nx']<=64 or
       (a['root0'] & 0x000ffffffffff000)!=a['low-table']+PAGE or
       (a['root0'] & 3)!=3 or (a['root128'] & 3)!=3 or
       ((a['root0']^a['oldroot0'])&~0x20)==0 or
       not test_target or not a['rx-target'] or not a['nx-target'] or
       a['rx-target']==a['nx-target'] or
       b['cr3']!=a['new'] or b['tr']!=a['selector'] or
       not IST1<=b['rsp']<=IST1+PAGE-40 or b['rsp']&7 or
       not b['saved-rip'] or b['saved-rip']!=b['expected-rip'] or
       p['cr3']!=a['new'] or p['tr']!=a['selector'] or
       p['low-table']!=a['low-table'] or
       ((p['root0']^a['root0'])&~0x20)!=0 or
       ((p['root128']^a['root128'])&~0x20)!=0 or
       p['cr2']!=test_target or p['target']!=test_target or
       p['err']!=expected_pf_code or
       not IST2<=p['rsp']<=IST2+PAGE-48 or p['rsp']&7 or
       not IST1<=p['old-rsp']<IST1+PAGE or
       p['rip']!=p['expected-rip'] or not p['rip'] or
       (mode==2 and p['rip']!=test_target) or
       (mode==1 and p['rip']==test_target) or
       (p['cr0'] & (1<<16))==0 or (p['efer'] & (1<<11))==0 or
       p['leaf-flags']!=(2 if mode==1 else 1)):
        return False,'mode/real PF/CR2/PFEC/RIP/IST2/CR3/WP/NXE/RX-NX layout mismatch'
    return True,('hardware supervisor RX write protection' if mode==1 else 'hardware NX instruction-fetch protection')+' confirmed via IST2 #PF'
