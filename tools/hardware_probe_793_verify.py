#!/usr/bin/env python3
"""Verify actual QEMU UART observations. Synthetic fixtures NEVER prove hardware."""
import re

FIELDS={
 'GOP':('base','bytes','width','height','stride','pixel','page-start','page-end'),
 'MAP':('descriptors','overlaps','covered','complete','mixed','type','attributes','runtime'),
 'CPU':('cr0','cr3','rflags','cpuid-edx','pat','mtrr-default','mtrr-supported'),
 'PORT':('com1','ps2-data','ps2-cmd','ata-primary','pic-master-mask','pic-slave-mask','if'),
}
POLICY=b'STYLEOS S793 POLICY cache-effective=unknown mmio-bars=unverified irq-routing=unverified firmware-runtime=unverified production-cr3=deny'
PATTERN=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')

def verify(raw:bytes,exit_code):
    if exit_code!=33 or len(raw)>300000 or b'STYLEOS S793 FAIL' in raw:
        return False,'exit/size/failure'
    lines=raw.splitlines()
    if lines.count(b'STYLEOS S793 PASS')!=1 or lines.count(POLICY)!=1:
        return False,'missing/duplicate markers'
    if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:
        return False,'missing handoff proof'
    result={}
    for name,fields in FIELDS.items():
        found=[line for line in lines if line.startswith(b'STYLEOS S793 '+name.encode()+b' ')]
        if len(found)!=1:return False,'missing/duplicate '+name
        tokens=PATTERN.findall(found[0])
        if len(tokens)!=len(fields) or [k.decode() for k,_ in tokens]!=list(fields):
            return False,'wrong '+name+' fields'
        if b' '.join(k+b'=0x'+v for k,v in tokens)!=found[0].split(b' ',3)[-1]:
            return False,'garbled '+name
        result[name]={k.decode():int(v,16) for k,v in tokens}
    g,m,c,p=(result[x] for x in FIELDS)
    if not (0<g['base']<1<<52 and 0<g['bytes']<=1<<32 and
        320<=g['width']<=4096 and 240<=g['height']<=4096 and
        g['width']<=g['stride']<=8192 and g['pixel'] in (0,1) and
        g['page-start']==g['base']&~4095 and
        g['page-end']==(g['base']+g['bytes']+4095)&~4095 and
        g['page-end']<=1<<52 and g['base']+g['bytes']<=g['page-end']):
        return False,'invalid GOP geometry'
    if not (0<m['descriptors']<=4096 and 0<m['overlaps']<=m['descriptors'] and
        0<=m['covered']<=g['page-end']-g['page-start'] and
        m['complete'] in (0,1) and m['mixed'] in (0,1) and
        (m['complete']==1)==(m['covered']==g['page-end']-g['page-start']) and
        m['type']<32 and m['runtime']<=m['descriptors']):
        return False,'invalid UEFI descriptor facts'
    if not (c['cr0']&(1<<31) and c['cr3'] and c['cpuid-edx']&(1<<5) and
        c['mtrr-supported']==int(bool(c['cpuid-edx']&(1<<12))) and
        c['pat'] and c['rflags']&(1<<1) and not c['rflags']&(1<<9)):
        return False,'invalid CPU observed state'
    if not (p['com1']==0x3f8 and p['ps2-data']==0x60 and
        p['ps2-cmd']==0x64 and p['ata-primary']==0x1f0 and
        p['if']==0 and p['pic-master-mask']<=255 and p['pic-slave-mask']<=255):
        return False,'invalid port/IRQ observation'
    positions=[lines.index(b'STYLEOS x86_64 ExitBootServices OK')]
    positions += [next(i for i,line in enumerate(lines) if line.startswith(b'STYLEOS S793 '+n.encode()+b' ')) for n in FIELDS]
    positions += [lines.index(POLICY),lines.index(b'STYLEOS S793 PASS')]
    if positions!=sorted(set(positions)) or len(positions)!=7:
        return False,'order'
    return True,result
