#!/usr/bin/env python3
"""Synthetic parser-only checks. No QEMU, no hardware acceptance."""
from fx_context_849_verify import verify_fx, CLEAN, POLICY, ORDER

def record(tag,fields):
    return ('STYLEOS S849 '+tag+' '+' '.join(f'{k}=0x{v:016x}' for k,v in fields.items())).encode()
roots={'safe':0x1000,'a':0x2000,'b':0x3000}
obs={'child_roots':roots}
lines=[b'STYLEOS S848 ADMIT example',b'STYLEOS S848 STOP example',record('MODE',{'ecx':0,'edx':0x07000001,'cr0':0x23,'cr4':0x600}),record('READY',{'mask':0xffbf,'safe':roots['safe']}),b'STYLEOS S848 READY example']
counts=[0,0]
for step,id in enumerate(ORDER):
    counts[id]+=1; tag=0x8480000000000000|(id<<32)
    lines.extend([record('CAPTURE',{'id':id,'root':roots['ab'[id]],'n':counts[id],'xmm14':tag|13,'xmm15':tag|14,'mxcsr':0x3f80 if id else 0x1f80,'st0':0 if id else 0x8000000000000000,'exp':0 if id else 0x3fff,'kernel-restored':1,'terminal':int(step in (6,9))}),f'STYLEOS S848 EVENT {step}'.encode(),f'STYLEOS S848 BACK {step}'.encode()])
lines.extend([CLEAN,b'STYLEOS S848 TOTAL example',b'STYLEOS S848 POLICY example',POLICY,b'STYLEOS S846 CLEAN example'])
assert len(verify_fx(lines,obs)['fx_captures'])==10
cases=[]
for tag,key in [('MODE','ecx'),('MODE','edx'),('MODE','cr0'),('MODE','cr4'),('READY','mask'),('READY','safe'),('CAPTURE','id'),('CAPTURE','root'),('CAPTURE','n'),('CAPTURE','xmm14'),('CAPTURE','xmm15'),('CAPTURE','mxcsr'),('CAPTURE','st0'),('CAPTURE','exp'),('CAPTURE','kernel-restored'),('CAPTURE','terminal')]:
    import re
    i=next(i for i,x in enumerate(lines) if x.startswith(('STYLEOS S849 '+tag+' ').encode()))
    bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+key.encode()+rb'=0x([0-9a-f]{16})',lambda m:key.encode()+b'=0x'+f'{int(m[1],16)^(1<<27 if key=="ecx" else 1<<24 if key=="edx" else 2 if key=="cr0" else 1<<18 if key=="cr4" else 1<<63):016x}'.encode(),bad[i],count=1)
    assert bad[i]!=lines[i];cases.append(bad)
for tag in (b'MODE',b'READY',b'CAPTURE',b'CLEAN',b'POLICY'):
    cases.append([x for x in lines if not x.startswith(b'STYLEOS S849 '+tag+b' ')])
bad=lines[:];bad[5],bad[6]=bad[6],bad[5];cases.append(bad)
for i,case in enumerate(cases):
    try:verify_fx(case,obs)
    except ValueError:continue
    raise AssertionError(i)
print(f'S849 synthetic parser: 1 valid, {len(cases)} refusals PASS (NOT guest evidence)')
