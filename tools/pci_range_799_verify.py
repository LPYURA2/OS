"""Strict per-boot PCI BAR sizing/restore hardware UART verifier, stage 799."""
import re
FIELDS={
 'PCI':('bus','device','function','vendor','id','bridge-bus','bridge-device','secondary'),
 'STATE':('command-before','command-after','decode-disabled','registers-restored'),
 'BAR':('index','width','prefetch','raw-lo','raw-hi','mask-lo','mask-hi','base','size','end','window-base','window-end'),
}
POLICY=(b'STYLEOS S799 POLICY range-proven=two-downstream-bars cache-effective=unknown '
        b'all-bars=unmeasured irq-routing=unknown firmware-runtime=unknown production-cr3=deny')
PAIR=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')

def verify(raw, rc):
    if rc!=33 or not isinstance(raw,bytes) or not raw or len(raw)>300000:
        return False,'missing hardware/debug-exit'
    lines=raw.splitlines()
    if b'STYLEOS S799 FAIL' in raw or lines.count(b'STYLEOS S799 PASS')!=1 or lines.count(POLICY)!=1 or lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:
        return False,'missing/duplicated mandatory markers'
    known=(b'STYLEOS S799 PCI ',b'STYLEOS S799 STATE ',b'STYLEOS S799 BAR ',
           b'STYLEOS S799 POLICY ',b'STYLEOS S799 PASS')
    if any(line.startswith(b'STYLEOS S799 ') and not any(line.startswith(k) for k in known) for line in lines):
        return False,'unexpected stage record'
    if sum(line.startswith(b'STYLEOS S799 POLICY ') for line in lines)!=1:
        return False,'extra policy record'
    records={}
    for tag,names in FIELDS.items():
        prefix=b'STYLEOS S799 '+tag.encode()+b' '
        found=[]
        for i,line in enumerate(lines):
            if not line.startswith(prefix):continue
            body=line[len(prefix):]
            kv=PAIR.findall(body)
            if (len(kv)!=len(names) or tuple(k.decode() for k,_ in kv)!=names or
                body!=b' '.join(k+b'=0x'+v for k,v in kv)):
                return False,'bad syntax '+tag
            found.append((i,{k.decode():int(v,16) for k,v in kv}))
        records[tag]=found
    if len(records['PCI'])!=1 or len(records['STATE'])!=1 or len(records['BAR'])!=2:
        return False,'record counts'
    order=[lines.index(b'STYLEOS x86_64 ExitBootServices OK'),records['PCI'][0][0],records['STATE'][0][0]]
    order.extend(i for i,_ in records['BAR'])
    order.extend((lines.index(POLICY),lines.index(b'STYLEOS S799 PASS')))
    if order!=sorted(set(order)):return False,'out-of-order log'
    ident=records['PCI'][0][1];state=records['STATE'][0][1];bars=[b for _,b in records['BAR']]
    if any(ident[k]!=v for k,v in {'bus':1,'device':0,'function':0,'vendor':0x1af4,'id':0x1043,'bridge-bus':0,'bridge-device':0x1c,'secondary':1}.items()):
        return False,'incorrect hardware identity/bridge path'
    if (not 0<state['command-before']<=0xffff or
        state['command-before']!=state['command-after'] or not(state['command-before']&7) or
        state['decode-disabled']!=1 or state['registers-restored']!=1):
        return False,'device state/restoration not proven'
    if [b['index'] for b in bars]!=[1,4]:return False,'selected BAR order'
    for b,w,p in zip(bars,(32,64),(0,1)):
        base=b['base'];size=b['size'];end=b['end'];lo=b['raw-lo'];hi=b['raw-hi'];mask=b['mask-lo']
        if b['width']!=w or b['prefetch']!=p or ((b['raw-lo']&8)!=0)!=bool(p) or b['window-base']>=b['window-end']:
            return False,'BAR type/window'
        if w==32:
            expect= (1<<32) - (mask&0xfffffff0)
            if b['raw-hi'] or b['mask-hi'] or (lo&0xf)!=(mask&0xf) or (lo&6):
                return False,'32-bit BAR flags'
            calc=lo&0xfffffff0
        else:
            expect=(-( (b['mask-hi']<<32)|(mask&0xfffffff0))) & ((1<<64)-1)
            if lo&6 != 4 or (lo&0xf)!=(mask&0xf):return False,'64-bit BAR flags'
            calc=(hi<<32)|(lo&0xfffffff0)
        if (not 4096<=size<=(1<<52) or size&(size-1) or size!=expect or
            base!=calc or base%size or end!=base+size or end>1<<52 or
            not b['window-base']<=base<end<=b['window-end']):
            return False,'invalid measured full BAR extent'
    if not(bars[0]['end']<=bars[1]['base'] or bars[1]['end']<=bars[0]['base']):
        return False,'overlapping resources'
    if any(b'production-cr3=allow' in line or b'cache-effective=verified' in line for line in lines):
        return False,'unsafe claim'
    return True,{'pci':ident,'state':state,'bars':bars,'production_cr3_authorized':False}
