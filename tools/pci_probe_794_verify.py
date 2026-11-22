#!/usr/bin/env python3
"""Fail-closed UART verifier; source is genuine QEMU only in guest runner."""
import re
F={
 'GOP':('base','bytes','width','height','descriptors','uefi-overlaps'),
 'SCAN':('bus','devices','display','match'),
 'PCI':('bus','device','function','vendor','id','class','subclass','command'),
 'BAR':('index','raw-lo','raw-hi','base','width','prefetch'),
}
POLICY=b'STYLEOS S794 POLICY bar-base-correlated=1 bar-size=unknown bar-aperture=unverified cache-effective=unknown irq-routing=unverified firmware-runtime=unverified production-cr3=deny'
TOKENS=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')
def verify(raw:bytes,rc):
    if rc!=33 or len(raw)>300000 or b'STYLEOS S794 FAIL' in raw or b'STYLEOS S794 GAP' in raw:
        return False,'exit, oversized, failure or gap'
    lines=raw.splitlines()
    start=b'STYLEOS x86_64 ExitBootServices OK';done=b'STYLEOS S794 PASS'
    if lines.count(start)!=1 or lines.count(done)!=1 or lines.count(POLICY)!=1:
        return False,'missing/duplicate stage policy markers'
    result={};positions=[lines.index(start)]
    for name,fields in F.items():
        prefix=b'STYLEOS S794 '+name.encode()+b' '
        found=[(i,line) for i,line in enumerate(lines) if line.startswith(prefix)]
        if len(found)!=1:return False,'missing/duplicate '+name
        ix,line=found[0]
        tail=line[len(prefix):];tokens=TOKENS.findall(tail)
        if (len(tokens)!=len(fields) or tuple(k.decode() for k,_ in tokens)!=fields or
            b' '.join(k+b'=0x'+v for k,v in tokens)!=tail):
            return False,'invalid fields '+name
        result[name]={k.decode():int(v,16) for k,v in tokens};positions.append(ix)
    positions.extend([lines.index(POLICY),lines.index(done)])
    if positions!=sorted(set(positions)):return False,'order'
    g,s,p,b=(result[k] for k in F)
    if not (0<g['base']<1<<52 and 0<g['bytes']<=1<<32 and
        320<=g['width']<=4096 and 240<=g['height']<=4096 and
        0<g['descriptors']<=4096 and 0<=g['uefi-overlaps']<=g['descriptors']):
        return False,'GOP bounds'
    if not (s['bus']==0 and 0<s['devices']<=256 and 1<=s['display']<=s['devices'] and s['match']==1):
        return False,'PCI bus-zero scan'
    if not (p['bus']==0 and p['device']<32 and p['function']<8 and
        0<p['vendor']<0xffff and 0<p['id']<=0xffff and p['class']==3 and
        p['subclass'] in (0,0x80) and p['command']<=0xffff):
        return False,'VGA PCI identity'
    if not (0<=b['index']<=5 and b['width'] in (32,64) and b['prefetch'] in (0,1) and
        b['base']==g['base'] and b['base']<1<<52 and not b['raw-lo']&1 and
        ((b['raw-lo']&~15)|(b['raw-hi']<<32 if b['width']==64 else 0))==b['base']):
        return False,'BAR base and width'
    if b['width']==32 and (b['raw-hi']!=0 or (b['raw-lo']&6)!=0):
        return False,'32-bit BAR encoding'
    if b['width']==64 and (b['raw-lo']&6)!=4:
        return False,'64-bit BAR encoding'
    if bool(b['raw-lo']&8)!=bool(b['prefetch']):return False,'BAR prefetch flag'
    return True,result
