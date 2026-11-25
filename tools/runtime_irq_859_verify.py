"""One real IRQ on the S858 owned-root chain; bind interrupted RIP to linked PE."""
import re
from fault_site import _range,_unpack
from runtime_arena_858_verify import verify as prior,POLICY as ARENA
POLICY=b'STYLEOS S859 POLICY irq0=yes eoi=yes idt-restored=yes route-restored=yes pit-restored=no runtime-intact=yes arena-intact=yes scheduler-switch=no production-cr3=deny'
KEYS=('root','idt','ist','frame','cs','rip','wait-lo','wait-hi','rsp','wait-rsp','flags','hits','isr','eoi','pin','low','high','svr','tpr','drained')

def wait_site(data,map_text):
    pe,=_unpack('<I',data,60,'PE pointer')
    if data[:2]!=b'MZ' or _range(data,pe,4,'PE')!=b'PE\0\0':raise ValueError('PE magic')
    machine,count,_,_,_,opt_size,_=_unpack('<HHIIIHH',data,pe+4,'COFF')
    opt=_range(data,pe+24,opt_size,'optional header')
    if machine!=0x8664 or not 0<count<=96 or opt_size<112 or _unpack('<H',opt,0,'magic')[0]!=0x20b:raise ValueError('PE format')
    base,=_unpack('<Q',opt,24,'image base');size,headers=_unpack('<II',opt,56,'image size');start=pe+24+opt_size
    bases=re.findall(r'^\s*Preferred load address is ([0-9a-fA-F]{16})\s*$',map_text,re.M)
    if bases!=[f'{base:016x}'] or not size or headers<start+40*count or headers>len(data):raise ValueError('map base')
    matches=[]
    for name in ('irq859_wait_lo','irq859_wait_hi'):
        rows=[l for l in map_text.splitlines() if name in l.split()]
        if len(rows)!=1:raise ValueError('symbol count')
        m=re.fullmatch(r'\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+boot_x86_64_irq_delivery_807\.o\s*',rows[0])
        if not m:raise ValueError('symbol row')
        seg,rel,absolute=(int(v,16) for v in m.groups());rva=absolute-base
        if not 1<=seg<=count or not 0<rva<size:raise ValueError('symbol bounds')
        sec,vs,va,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',data,start+40*(seg-1),'section')
        if sec.rstrip(b'\0')!=b'.text' or not flags&0x20000000 or flags&0x80000000 or rva!=va+rel or not 1<=rel<min(vs or rs,rs) or ro<headers:raise ValueError('symbol section')
        _range(data,ro,rs,'section bytes');matches.append((rva,ro+rel))
    lo,hi=matches
    if hi[0]-lo[0]!=2 or hi[1]-lo[1]!=2 or _range(data,lo[1]-1,3,'STI/NOP/CLI')!=b'\xfb\x90\xfa':raise ValueError('wait opcodes')
    return {'wait_lo_rva':lo[0],'wait_hi_rva':hi[0]}

def verify(raw,rc,site,reference):
    try:
        ok,obs=prior(raw,rc,site['rva'],reference)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[x for x in lines if x.startswith(b'STYLEOS S859 ')]
        if len(rows)!=2 or rows[1]!=POLICY or not lines.index(ARENA)<lines.index(rows[0])<lines.index(POLICY):raise ValueError('IRQ record order')
        m=re.fullmatch(b'STYLEOS S859 IRQ '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in KEYS),rows[0])
        if not m:raise ValueError('IRQ schema')
        v=dict(zip(KEYS,(int(x,16) for x in m.groups())))
        def extent(tag):
            rows=[l for l in lines if l.startswith(('STYLEOS MEMMAP '+tag+' ').encode())]
            if len(rows)!=1:raise ValueError('extent')
            return tuple(int(x,16) for x in rows[0].split()[-2:])
        im,end=extent('IMAGE');st,en=extent('STACK');lo,hi=im+site['wait_lo_rva'],im+site['wait_hi_rva']
        if not im<lo<hi<end or hi-lo!=2 or (v['wait-lo'],v['wait-hi'])!=(lo,hi) or not lo<=v['rip']<hi:raise ValueError('interrupted site')
        if (v['root'],v['idt'],v['ist'],v['frame'],v['cs'])!=(obs['active']['root'],obs['active']['idt'],0x400000003000,obs['bp']['frame'],obs['bp']['cs']):raise ValueError('root or IST binding')
        if v['rsp']!=v['wait-rsp'] or v['rsp']&7 or not st<=v['rsp']<en or v['flags']&0x202!=0x202 or v['flags']&~0xad7:raise ValueError('interrupted frame')
        if tuple(v[k] for k in ('hits','isr','eoi','pin','low','high','svr','tpr','drained'))!=(1,1,1,2,0x10000,0,0x10f,0,1):raise ValueError('delivery/restoration')
        obs['irq']=v;return True,obs
    except (ValueError,KeyError,TypeError,IndexError,StopIteration) as exc:return False,str(exc)
