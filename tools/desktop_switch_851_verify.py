"""Strict terminal switch evidence; preflight still mandatory, not production."""
import re
from desktop_preflight_850_verify import verify as preflight, POLICY as PRE
POLICY=b'STYLEOS S851 POLICY terminal=yes own-root=yes hardware-ist=yes framebuffer-restored=yes firmware-return=no production-cr3=deny'
CLEAN_POLICY=b'STYLEOS S851 POLICY own-root=yes descriptors-live=yes ist-mapped=yes firmware-return=no probes=absent'
def verify(raw,rc,bp_rva,*,host_quit=False,clean_boot=False):
    try:
        ok,data=preflight(raw,rc,host_quit=host_quit)
        if not ok:raise ValueError(data)
        lines=raw.splitlines();records=[x for x in lines if x.startswith(b'STYLEOS S851 ')]
        if len(records)!=(3 if clean_boot else 6) or records[-1]!=(CLEAN_POLICY if clean_boot else POLICY) or any(b' FAIL ' in x for x in lines):raise ValueError('record/policy/failure')
        if lines.index(PRE)>=lines.index(records[0]) or [lines.index(x) for x in records]!=sorted(lines.index(x) for x in records):raise ValueError('phase order')
        def row(line,tag,keys):
            prefix=('STYLEOS S851 '+tag+' ').encode();m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):]) if line.startswith(prefix) else None
            if not m:raise ValueError('schema '+tag)
            return dict(zip(keys,(int(x,16) for x in m.groups())))
        def region(tag):
            prefix=b'STYLEOS MEMMAP '+tag+b' ';v=[x for x in lines if x.startswith(prefix)]
            if len(v)!=1:raise ValueError('region')
            return tuple(int(x,16) for x in v[0][len(prefix):].split())
        g,r,d=data['gop'],data['root'],data['descriptors'];lo,hi=region(b'IMAGE');st,en=region(b'STACK')
        b=row(records[0],'BRIDGE',('idt','old','new','cs','selector'))
        occupied=[(r['detached'],r['detached']+32*4096),(r['source'],r['source']+32*4096),(st,en),region(b'HANDOFF'),region(b'SOURCE')]+[(v,v+4096) for v in d.values()]
        if not lo<=b['idt']<b['idt']+4096<=hi or b['idt']%4096 or any(b['idt']<y and x<b['idt']+4096 for x,y in occupied):raise ValueError('bridge provenance')
        if (b['old'],b['new'])!=(r['inherited'],r['detached']) or not 0<b['cs']<b['selector']<=4080 or (b['cs']|b['selector'])&7:raise ValueError('bridge transition')
        a=row(records[1],'ACTIVE',('root','gdt','idt','tr','pat','pge'))
        if tuple(a.values())!=(r['detached'],d['gdt'],d['idt'],b['selector'],r['pat'],0):raise ValueError('active descriptors')
        if clean_boot:
            if bp_rva is not None:raise ValueError('unexpected boot probe site')
            data.update(bridge=b,active=a,boot_probes=False)
            return True,data
        bp=row(records[2],'BP',('root','frame','rip','rsp','cs','flags'))
        if not isinstance(bp_rva,int) or not 0<bp_rva<hi-lo or (bp['root'],bp['frame'],bp['rip'],bp['cs'])!=(r['detached'],0x400000003000+4096-16-40,lo+bp_rva,b['cs']) or not st<=bp['rsp']<en or bp['flags']&0x200 or not bp['flags']&2:raise ValueError('hardware frame')
        pixels=[]
        for i,off in enumerate((0,4*((g['height']-1)*g['stride']+g['width']-1))):
            v=row(records[3+i],'PIXEL',('id','root','address','old','write','read','restored'))
            if (v['id'],v['root'],v['address'])!=(i,r['detached'],g['base']+off) or off+4>g['bytes'] or any(v[k]>0xffffffff for k in ('old','write','read','restored')) or v['write']!=v['old']^0xffffff or v['read']!=v['write'] or v['restored']!=v['old']:raise ValueError('pixel roundtrip')
            pixels.append(v)
        data.update(bridge=b,active=a,bp=bp,pixels=pixels)
        return True,data
    except (ValueError,IndexError,KeyError,TypeError) as e:return False,str(e)
