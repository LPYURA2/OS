"""Bounded registry-only context-bank guest proof, retaining all S861 checks."""
import re
from runtime_timer_861_verify import verify as prior, timer_site, trace_check
from fault_site import _unpack,_range

def bank_site(efi,map_file):
    site=timer_site(efi,map_file);s=map_file.read_text()
    if re.search(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(?:ctl848|saved860|have860)\s+',s,re.M):raise ValueError('S862B duplicate bank')
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    for name,obj in [(f'rt862_bank_{n}',r'kernel_runtime_bank_862\.o') for n in ('init','enter','tick','finish')]+[('bank862',r'boot_x86_64_user_preemption_848\.o')]:
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S862B map '+name)
        seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S862B section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel:raise ValueError('S862B RVA')
        if name=='bank862':
            if not 0<=rel<vs or not flags&0x80000000 or flags&0x20000000:raise ValueError('S862B bank rights')
        else:
            if not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S862B code rights')
            _range(efi,ro+rel,1,'S862B entry byte')
        site[name+'_rva']=rva
    return site

def verify(raw,rc,site,reference,*,budgets=(3,5)):
    try:
        ok,obs=prior(raw,rc,site,reference,budgets=budgets)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S862B ')]
        def indices(tag):return [i for i,x in enumerate(lines) if x.startswith(tag)]
        frames=indices(b'STYLEOS S861T FRAME ');bound=indices(b'STYLEOS S861T BOUNDARY ');backs=indices(b'STYLEOS S848 BACK ');events=indices(b'STYLEOS S860 EVENT ')
        if len(rows)!=21 or any(len(x)!=10 for x in (frames,bound,backs,events)) or indices(b'STYLEOS S862 SELECT '):raise ValueError('S862B count')
        def row(i,tag,keys):
            m=re.fullmatch(b'STYLEOS S862B '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S862B schema')
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        saved=[(0x10000000,0x11001000,2)]*2;entries=[];commits=[];ti=0
        ready=indices(b'STYLEOS S848 READY ');scrub=indices(b'STYLEOS S861T SCRUB ');clean=indices(b'STYLEOS S860 CLEAN ')
        if any(len(x)!=1 for x in (ready,scrub,clean)):raise ValueError('S862B phase count')
        for step,e in enumerate(obs['runtime_ticks']):
            i=e['id'];a,b=rows[2*step:2*step+2];en=row(a,b'ENTER',('step','id','pid','dispatch','safe','root','pc','sp','flags'))
            pc,sp,flags=saved[i]
            if tuple(en.values())!=(step,i,e['pid'],e['dispatch'],e['safe'],e['root'],pc,sp,flags|0x200) or not (backs[step-1] if step else ready[0])<a<frames[step]<b<bound[step]<events[step]<backs[step]:raise ValueError('S862B entry/phase')
            terminal=e['state'] in (4,5)
            if terminal:saved[i]=(0,0,0)
            else:
                t=obs['user_preempt_ticks'][ti];ti+=1;saved[i]=(t['rip'],t['rsp'],t['flags']&~0x200)
            co=row(b,b'COMMIT',('step','id','pid','state','pc','sp','flags','zero'))
            if tuple(co.values())!=(step,i,e['pid'],e['state'],*saved[i],int(terminal)):raise ValueError('S862B commit')
            entries.append(en);commits.append(co)
        end=row(rows[-1],b'CLEAN',('safe','bank-zero','pending-zero'))
        if tuple(end.values())!=(obs['active']['root'],1,1) or not scrub[0]<rows[-1]<clean[0] or saved!=[(0,0,0)]*2:raise ValueError('S862B cleanup')
        obs.update(bank_entries=entries,bank_commits=commits,bank_clean=end);return True,obs
    except (ValueError,KeyError,TypeError,IndexError,StopIteration) as e:return False,str(e)
