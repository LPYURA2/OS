"""Strict UART evaluator. Firmware QEMU run is provided by test_pci_aperture_795.py."""
import re
TOKENS=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')
POLICY=b'STYLEOS S795 POLICY range=verified cache-effective=unknown all-pci=unverified irq-routing=unverified firmware-runtime=unverified production-cr3=deny'
FIELDS={
 'GOP':('base','bytes','width','height','uefi-overlaps'),
 'PCI':('bus','device','function','vendor','id','devices','index','width','base'),
 'SIZING':('cmd-before','cmd-restored','mask-lo','mask-hi','decode-disabled','registers-restored','size','end'),
}
def verify(data,rc):
    if rc!=33 or not data or len(data)>300000 or b'STYLEOS S795 FAIL' in data:
        return False,'exit/failure/oversize'
    lines=data.splitlines()
    anchors=[b'STYLEOS x86_64 ExitBootServices OK',POLICY,b'STYLEOS S795 PASS']
    if any(lines.count(x)!=1 for x in anchors):return False,'missing/duplicated policy or markers'
    points=[lines.index(anchors[0])];out={}
    for label,fields in FIELDS.items():
        prefix=b'STYLEOS S795 '+label.encode()+b' '
        matches=[(i,ln[len(prefix):]) for i,ln in enumerate(lines) if ln.startswith(prefix)]
        if len(matches)!=1:return False,'missing/duplicate '+label
        i,body=matches[0];pairs=TOKENS.findall(body)
        if (len(pairs)!=len(fields) or tuple(k.decode() for k,_ in pairs)!=fields or
            body!=b' '.join(k+b'=0x'+v for k,v in pairs)):
            return False,'format '+label
        points.append(i);out[label]={k.decode():int(v,16) for k,v in pairs}
    points += [lines.index(POLICY),lines.index(anchors[2])]
    if points!=sorted(set(points)):return False,'order'
    g,p,s=out['GOP'],out['PCI'],out['SIZING']
    if not (g['base']>0 and g['base']==p['base'] and 0<g['bytes']<=1<<32 and
        320<=g['width']<=4096 and 240<=g['height']<=4096 and g['uefi-overlaps']==0):
        return False,'GOP geometry'
    if not (p['bus']==0 and p['device']<32 and p['function']<8 and
        p['vendor'] not in (0,0xffff) and 0<p['id']<=0xffff and
        1<=p['devices']<=256 and 0<=p['index']<=5 and p['width'] in (32,64)):
        return False,'PCI identity'
    if not (s['cmd-before']==s['cmd-restored'] and s['cmd-before']<=0xffff and
        s['decode-disabled']==1 and s['registers-restored']==1 and
        s['size']>=4096 and s['size']&(s['size']-1)==0 and
        p['base']%s['size']==0 and s['end']==p['base']+s['size'] and
        p['base']+g['bytes']<=s['end']):
        return False,'restoration/range'
    if p['width']==32:
        if s['mask-hi']!=0 or s['size'] != (1<<32)-(s['mask-lo']&0xfffffff0):
            return False,'32bit mask'
    else:
        if s['size'] != ((~((s['mask-hi']<<32)|(s['mask-lo']&0xfffffff0)))&((1<<64)-1))+1:
            return False,'64bit mask'
    return True,out
