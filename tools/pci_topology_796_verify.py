"""Strict evidence checker for terminal, read-only PCI topology UART."""
import re
TOKENS=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')
POLICY=(b'STYLEOS S796 POLICY pci-configuration=read-only bar-sizes=unknown '
        b'bridge-window-sizes=observed-not-owned cache-effective=unknown '
        b'irq-routing=unverified firmware-runtime=unverified production-cr3=deny')
FIELDS={
    'DEVICE':('bus','device','function','vendor','id','class','subclass','header'),
    'BRIDGE':('primary','secondary','subordinate','memory-base','memory-end','prefetch-base','prefetch-end'),
    'BAR':('bus','device','function','index','memory','base','width','prefetch'),
    'INVENTORY':('buses','devices','bridges','routes','bridge-windows','memory-bars','io-bars','bar-sizes-unknown','display-matches'),
    'GOP':('base','bytes','bus','device','function','observed-nonzero')}

def verify(data,rc,require_bridge=False):
    if rc!=33 or not data or len(data)>500000 or b'STYLEOS S796 FAIL' in data:
        return False,'exit/failure/oversize'
    lines=data.splitlines();a=b'STYLEOS x86_64 ExitBootServices OK';z=b'STYLEOS S796 PASS'
    if lines.count(a)!=1 or lines.count(z)!=1 or lines.count(POLICY)!=1:
        return False,'missing/duplicate markers'
    out={}
    for label,fields in FIELDS.items():
        prefix=b'STYLEOS S796 '+label.encode()+b' '
        records=[]
        for i,line in enumerate(lines):
            if not line.startswith(prefix):continue
            body=line[len(prefix):];parts=TOKENS.findall(body)
            if (len(parts)!=len(fields) or tuple(k.decode() for k,_ in parts)!=fields
                or body!=b' '.join(k+b'=0x'+v for k,v in parts)):
                return False,'malformed '+label
            records.append((i,{k.decode():int(v,16) for k,v in parts}))
        out[label]=records
    if (len(out['INVENTORY'])!=1 or len(out['GOP'])!=1 or
        not lines.index(a)<out['INVENTORY'][0][0]<out['GOP'][0][0]<lines.index(POLICY)<lines.index(z)):
        return False,'order/count'
    stats=out['INVENTORY'][0][1];g=out['GOP'][0][1]
    devices=out['DEVICE'];bridges=out['BRIDGE'];bars=out['BAR']
    if not(1<=stats['buses']<=256 and stats['devices']==len(devices) and
           stats['bridges']==len(bridges) and stats['routes']==len(bridges) and
           stats['bridge-windows']==sum(x['memory-end']!=0 for _,x in bridges) and
           stats['memory-bars']==sum(x['memory']==1 for _,x in bars) and
           stats['io-bars']==sum(x['memory']==0 for _,x in bars) and
           stats['bar-sizes-unknown']==len(bars) and stats['display-matches']==1 and
           stats['memory-bars']>=1 and g['base']>0 and 0<g['bytes']<=1<<32 and
           g['observed-nonzero']==1):
        return False,'census totals'
    if require_bridge and (not bridges or stats['buses']<2):return False,'missing hardware bridge'
    keys=set();bus_ids={0};valid_devices=set()
    for _,b in bridges:
        if (b['primary'] not in bus_ids or not 1<=b['secondary']<=b['subordinate']<=255 or
            b['secondary'] in bus_ids or
            (b['memory-base']!=0 and not b['memory-base']<b['memory-end']) or
            (b['prefetch-base']!=0 and not b['prefetch-base']<b['prefetch-end'])):
            return False,'bridge range/route'
        bus_ids.add(b['secondary'])
    # The QEMU traversal logs buses in queue order; the print-order may include
    # bridge records after DEVICE lines. Count, do not invent nonexistent buses.
    if len(bus_ids)!=stats['buses']:return False,'bus traversal count'
    for _,d in devices:
        key=(d['bus'],d['device'],d['function'])
        if (key in keys or d['bus'] not in bus_ids or d['device']>=32 or d['function']>=8 or
            d['vendor'] in (0,65535) or not 0<d['id']<=65535 or d['header'] not in (0,1)):
            return False,'invalid device identity'
        keys.add(key);valid_devices.add(key)
    for _,bar in bars:
        key=(bar['bus'],bar['device'],bar['function'])
        if key not in valid_devices or bar['index']>=6 or bar['width'] not in (32,64) or not bar['base'] or bar['memory']>1 or bar['prefetch']>1:
            return False,'invalid BAR observation'
    gkey=(g['bus'],g['device'],g['function'])
    if gkey not in valid_devices or sum(x['base']==g['base'] and x['memory']==1 and
            (x['bus'],x['device'],x['function'])==gkey for _,x in bars)!=1:
        return False,'unverified display BAR'
    if any(b'production-cr3=allow' in x for x in lines):return False,'unsafe authorization'
    return True,{'inventory':stats,'gop':g,'devices':len(devices),'bridges':len(bridges),'bars':len(bars)}
