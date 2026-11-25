"""Independent S861 shared Desktop / LAPIC / FP evidence; no synthetic fallback."""
import re
from desktop_switch_851_site import bp_site
from fault_site import _unpack,_range
from runtime_roots_861_verify import verify as roots_verify,trace_check as roots_trace,HEADER
from user_preempt_848_verify import verify_stage,ORDER,fixture_order
from fx_context_849_verify import verify_fx,verify_link_map
from runtime_tick_860_verify import parse as parse860,KEYS,POLICY

def timer_site(efi,map_file):
    verify_link_map(map_file)
    s=map_file.read_text();site=bp_site(efi,s);site['child_rva']=bp_site(efi,s,symbol='s861_bp_return')['rva']
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF');base=site['preferred_base']
    def symbol(name,obj):
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S861T map '+name)
        seg,rel,va=map(lambda x:int(x,16),rows[0]);rva=va-base
        if not 1<=seg<=count:raise ValueError('S861T section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<max(vs,rs):raise ValueError('S861T symbol bounds')
        return rva,ro+rel,flags
    site['timer_idt_rva'],_,flags=symbol('idt848',r'boot_x86_64_user_preemption_848\.o')
    if not flags&0x80000000 or flags&0x20000000:raise ValueError('S861T IDT rights')
    for id,last in (('a','fault'),('b','exit')):
        _,off,flags=symbol(f's848_{id}_{last}',r'arch_x86_64_process_entry_847\.o')
        if not flags&0x20000000 or flags&0x80000000:raise ValueError('S861T code rights')
        if id=='a' and _range(efi,off,3,'fault instruction')!=b'\x48\x8b\x00':raise ValueError('S861T fault bytes')
        if id=='b' and _range(efi,off-2,2,'INT80')!=b'\xcd\x80':raise ValueError('S861T INT80 bytes')
    return site

def verify(raw,rc,site,reference,*,budgets=(3,5)):
    try:
        ok,obs=roots_verify(raw,rc,site,reference)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();r=obs['join_roots'];a=obs['active'];safe=r['safe'];obs['child_roots']=r
        obs['process_selectors']={'cs':0x5b,'ss':0x63}
        obs.update(verify_stage(lines,obs,fx=True,before_tag=b'STYLEOS S861 SWITCH id=0x0000000000000001 ',after_tag=b'STYLEOS S861T SCRUB ',budgets=budgets))
        obs.update(verify_fx(lines,obs,after_tag=b'STYLEOS S861T SCRUB ',budgets=budgets))
        rows=[x for x in lines if x.startswith(b'STYLEOS S861T ')];pr=[x for x in lines if x.startswith(b'STYLEOS S860 ')]
        if len(rows)!=22 or len(pr)!=13 or pr[-1]!=POLICY:raise ValueError('S861T record count')
        def row(line,tag,keys):
            m=re.fullmatch(b'STYLEOS S861T '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m:raise ValueError('S861T schema')
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        def pos(prefix):return next(i for i,x in enumerate(lines) if x.startswith(prefix))
        image=int(lines[pos(b'STYLEOS MEMMAP IMAGE ')].split()[-2],16)
        ready=row(rows[0],b'READY',('safe','idt','cs','ss','limit'))
        admission=parse860(pr[0],b'READY',('safe','pid-a','pid-b','handle-a','handle-b','win-a','win-b'))
        handles=(r['gen-a'],r['gen-b'])
        if tuple(ready.values())!=(safe,image+site['timer_idt_rva'],0x5b,0x63,0x67) or tuple(admission.values())!=(safe,1,2,*handles,1,2) or not lines.index(pr[0])<lines.index(rows[0])<pos(b'STYLEOS S848 READY '):raise ValueError('S861T admission')
        events=[x for x in lines if x.startswith((b'STYLEOS S848 TICK ',b'STYLEOS S848 END '))];backs=[x for x in lines if x.startswith(b'STYLEOS S848 BACK ')]
        ticks=[0,0];frames=[];boundaries=[];records=[];ti=0
        for step,(id,terminal) in enumerate(fixture_order(budgets)):
            if not terminal:ticks[id]+=1
            f=row(rows[1+2*step],b'FRAME',('step','flags'))
            if f['step']!=step or not f['flags']&2 or f['flags']&~0xad7 or (not terminal and f['flags']!=obs['user_preempt_ticks'][ti]['flags']):raise ValueError('S861T flags')
            if not terminal:ti+=1
            b=row(rows[2+2*step],b'BOUNDARY',('step','safe','gdt','idt','pid'));v=parse860(pr[step+1],b'EVENT',KEYS)
            if tuple(b.values())!=(step,safe,a['gdt'],a['idt'],id+1) or tuple(v.values())!=(step,id,id+1,handles[id],step+1,safe,r['ab'[id]],ticks[id],(4 if id else 5) if terminal else 1,12288,0 if terminal else id+1,0 if terminal else int(id==0)):raise ValueError('S861T dispatch')
            if not lines.index(rows[1+2*step])<lines.index(events[step])<lines.index(rows[2+2*step])<lines.index(pr[step+1])<lines.index(backs[step]):raise ValueError('S861T order')
            frames.append(f);boundaries.append(b);records.append(v)
        scrub=row(rows[-1],b'SCRUB',('safe','used','tables-zero','leases-zero'));clean=parse860(pr[-2],b'CLEAN',('safe','dispatches','processes','windows'))
        if tuple(scrub.values())!=(safe,0,1,1) or tuple(clean.values())!=(safe,10,0,0) or ticks!=list(budgets) or not lines.index(backs[-1])<pos(b'STYLEOS S849 POLICY ')<lines.index(rows[-1])<lines.index(pr[-2])<lines.index(POLICY)<pos(b'STYLEOS S861 CLEAN '):raise ValueError('S861T cleanup')
        obs.update(timer_ready=ready,timer_frames=frames,timer_boundaries=boundaries,timer_scrub=scrub,runtime_ticks=records,runtime_clean=clean,runtime_admission=admission);return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)

def trace_check(text,obs,*,budgets=(3,5)):
    h=list(HEADER.finditer(text));r=obs['join_roots'];roots=(r['safe'],r['a'],r['b']);start=None
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)];cr=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if len(cr)!=1:raise ValueError('S861T CR3')
        if int(cr[0],16) in roots:start=i;break
    if start is None or len(h)-start!=13:raise ValueError('S861T event count')
    records=roots_trace(text[:h[start+3].start()],obs);a=obs['active'];ti=ei=0
    for step,(id,terminal) in enumerate(fixture_order(budgets)):
        soft=int(terminal and id==1)
        if terminal:p=obs['user_preempt_ends'][ei];ei+=1;vec=p['vector'];error=p['error'];rip=p['rip']-2*soft
        else:p=obs['user_preempt_ticks'][ti];ti+=1;vec=0x50;error=0;rip=p['rip']
        i=start+3+step;m=h[i];block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)]
        def val(pattern):
            v=re.findall(pattern,block,re.M)
            if len(v)!=1:raise ValueError('S861T CPU field '+pattern)
            return int(v[0],16)
        rec=tuple(int(v,16) for v in m.groups()[1:])+(val(r'\bCR3=([0-9a-fA-F]+)'),val(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)'),val(r'^RIP=([0-9a-fA-F]+)'),val(r'\bRSP=([0-9a-fA-F]+)'),val(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^TR\s*=([0-9a-fA-F]+)\b'))
        want=(vec,error,soft,3,0x5b,rip,rip,0x63,0x11001000,r['ab'[id]],obs['timer_frames'][step]['flags'],rip,0x11001000,a['gdt'],obs['timer_ready']['idt'],a['tr'])
        if rec!=want or int(m[1])!=records[-1][0]+1:raise ValueError('S861T CPU binding')
        if vec==14 and val(r'^CR0=[^\n]*\bCR2=([0-9a-fA-F]+)')!=p['cr2']:raise ValueError('S861T CR2')
        records.append((int(m[1]),rec))
    return records
