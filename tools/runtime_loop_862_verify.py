"""S862 registry-driven bounded loop, independent fixed fixture expectations."""
import re,struct
from runtime_bank_862_verify import verify as prior, bank_site, trace_check as prior_trace
from fault_site import _unpack,_range
BUDGETS=(5,3)

def loop_site(efi,map_file):
    site=bank_site(efi,map_file);s=map_file.read_text()
    rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+s862_loop_budget\s+([0-9a-fA-F]{16})\s+boot_x86_64_user_preemption_848\.o\s*$',s,re.M)
    if len(rows)!=1:raise ValueError('S862L budget symbol')
    seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    if not 1<=seg<=count:raise ValueError('S862L section')
    _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
    if rva!=v+rel or not 0<=rel<=min(vs,rs)-8 or not flags&0x40000000 or flags&0xa0000000 or _range(efi,ro+rel,8,'S862L budgets')!=struct.pack('<II',*BUDGETS):raise ValueError('S862L linked budget bytes/rights')
    site['loop_budget_rva']=rva;site['loop_budgets']=list(BUDGETS);return site

def verify(raw,rc,site,reference,*,sparse=False):
    try:
        if site.get('loop_budgets')!=list(BUDGETS) or not site.get('loop_budget_rva'):raise ValueError('S862L binary policy')
        ok,obs=prior(raw,rc,site,reference,budgets=BUDGETS,sparse=sparse)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S862L ')]
        def pos(prefix):
            found=[i for i,x in enumerate(lines) if x.startswith(prefix)]
            if len(found)!=1:raise ValueError('S862L phase '+str(prefix))
            return found[0]
        def row(i,tag,keys):
            m=re.fullmatch(b'STYLEOS S862L '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S862L schema')
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        if len(rows)!=2:raise ValueError('S862L count')
        ready=row(rows[0],b'READY',('a','b','watchdog'));empty=row(rows[1],b'EMPTY',('dispatches','safe','windows'))
        events=obs['runtime_ticks'];terminal=[(e['id'],e['state']) for e in events if e['state'] in (4,5)]
        if tuple(ready.values())!=(*BUDGETS,32) or tuple(empty.values())!=(len(events),obs['active']['root'],0) or terminal!=[(1,4),(0,5)]:raise ValueError('S862L lifecycle')
        if not pos(b'STYLEOS S848 READY ')<rows[0]<pos(b'STYLEOS S862B ENTER step=0x0000000000000000 ') or not pos(b'STYLEOS S848 BACK step=0x0000000000000009 ')<rows[1]<pos(b'STYLEOS S849 CLEAN '):raise ValueError('S862L phase ordering')
        obs.update(loop_ready=ready,loop_empty=empty);return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)

def trace_check(text,obs):return prior_trace(text,obs,budgets=BUDGETS)
