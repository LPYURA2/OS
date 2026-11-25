"""Bind cooperative CPL3 on the actual S861 Desktop root, no timer claim."""
import re
from desktop_switch_851_site import bp_site
from fault_site import _unpack,_range
from runtime_roots_861_verify import verify as roots_verify,trace_check as roots_trace,HEADER
from process_trial_847_verify import verify as process_verify,verify_link_map
POLICY=b'STYLEOS S861U POLICY cooperative-cpl3=yes existing-dispatcher=yes timer=no fp=no production-cr3=deny'
def user_site(efi,map_file):
    verify_link_map(map_file)
    s=map_file.read_text();site=bp_site(efi,s);site['child_rva']=bp_site(efi,s,symbol='s861_bp_return')['rva']
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF');base=site['preferred_base']
    def symbol(name,obj):
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S861U map '+name)
        seg,rel,va=map(lambda x:int(x,16),rows[0]);rva=va-base
        if not 1<=seg<=count:raise ValueError('S861U section')
        n,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<max(vs,rs):raise ValueError('S861U symbol bounds')
        return rva,ro+rel,flags
    site['user_idt_rva'],_,flags=symbol('idt847',r'boot_x86_64_process_trial_847\.o')
    if not flags&0x80000000 or flags&0x20000000:raise ValueError('S861U IDT rights')
    for i,last in ((0,'fault'),(1,'exit')):
        for tag in ('ok','deny','yield',last):
            _,off,flags=symbol(f's847_{i}_{tag}',r'arch_x86_64_process_entry_847\.o')
            if not flags&0x20000000 or flags&0x80000000:raise ValueError('S861U code rights')
            if tag=='fault':
                if _range(efi,off,3,'fault instruction')!=b'\x48\x8b\x03':raise ValueError('S861U fault bytes')
            elif _range(efi,off-2,2,'INT80')!=b'\xcd\x80':raise ValueError('S861U INT80 bytes')
    return site
def verify(raw,rc,site,reference):
    try:
        ok,obs=roots_verify(raw,rc,site,reference)
        if not ok:raise ValueError(obs)
        ok,obs=process_verify(raw,rc,joined=obs)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[x for x in lines if x.startswith(b'STYLEOS S861U ')];p847=[x for x in lines if x.startswith(b'STYLEOS S847 ')]
        if len(rows)!=15 or rows[-1]!=POLICY:raise ValueError('S861U records')
        def row(line,tag,keys):
            m=re.fullmatch(b'STYLEOS S861U '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m:raise ValueError('S861U schema')
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        def pos(prefix):return next(i for i,x in enumerate(lines) if x.startswith(prefix))
        r=obs['join_roots'];a=obs['active'];image=int(lines[pos(b'STYLEOS MEMMAP IMAGE ')].split()[-2],16)
        ready=row(rows[0],b'READY',('safe','pid-a','pid-b','win-a','win-b','idt'))
        if tuple(ready.values())!=(r['safe'],1,2,1,2,image+site['user_idt_rva']) or not pos(b'STYLEOS S861 SWITCH id=0x0000000000000001 ')<lines.index(rows[0])<lines.index(p847[0]):raise ValueError('S861U ready')
        frames=[];events=[];idx=1;k=0;j=1
        for step in range(4):
            for _ in range(3 if step<2 else 1):
                f=row(rows[idx],b'FRAME',('n','flags'))
                if f['n']!=k or not f['flags']&2 or f['flags']&~0x8d7 or not lines.index(rows[idx])<lines.index(p847[j])<lines.index(rows[idx+1]):raise ValueError('S861U frame')
                frames.append(f);idx+=1;k+=1;j+=1
            event=row(rows[idx],b'EVENT',('step','id','pid','handle','safe','gdt','idt','dispatch','resident','windows'))
            id=step%2
            if tuple(event.values())!=(step,id,id+1,r['gen-'+ 'ab'[id]],r['safe'],a['gdt'],a['idt'],step+1,12288,2 if step<2 else 1 if step==2 else 0) or not lines.index(rows[idx])<lines.index(p847[j]):raise ValueError('S861U event')
            events.append(event);idx+=1;j+=1
        clean=row(rows[idx],b'CLEAN',('safe','dispatches','processes','windows'))
        if tuple(clean.values())!=(r['safe'],4,0,0) or not lines.index(p847[-1])<lines.index(rows[idx])<lines.index(POLICY)<pos(b'STYLEOS S861 CLEAN '):raise ValueError('S861U clean')
        obs.update(user_ready=ready,user_frames=frames,user_events=events,user_clean=clean);return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)
def trace_check(text,obs):
    h=list(HEADER.finditer(text));r=obs['join_roots'];roots=(r['safe'],r['a'],r['b']);start=None
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)];cr=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if len(cr)!=1:raise ValueError('S861U CR3')
        if int(cr[0],16) in roots:start=i;break
    if start is None or len(h)-start!=11:raise ValueError('S861U event count')
    records=roots_trace(text[:h[start+3].start()],obs);a=obs['active']
    for j,(p,f) in enumerate(zip(obs['process_traps'],obs['user_frames'])):
        i=start+3+j;m=h[i];block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)]
        def val(pattern):
            v=re.findall(pattern,block,re.M)
            if len(v)!=1:raise ValueError('S861U CPU field '+pattern)
            return int(v[0],16)
        soft=int(p['vector']==128);rip=p['rip']-2*soft
        rec=tuple(int(v,16) for v in m.groups()[1:])+(val(r'\bCR3=([0-9a-fA-F]+)'),val(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)'),val(r'^RIP=([0-9a-fA-F]+)'),val(r'\bRSP=([0-9a-fA-F]+)'),val(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^TR\s*=([0-9a-fA-F]+)\b'))
        want=(p['vector'],p['error'],soft,3,p['cs'],rip,rip,p['ss'],p['rsp'],p['root'],f['flags'],rip,p['rsp'],a['gdt'],obs['user_ready']['idt'],a['tr'])
        if rec!=want or int(m[1])!=records[-1][0]+1:raise ValueError('S861U CPU event binding')
        if not soft and val(r'^CR0=[^\n]*\bCR2=([0-9a-fA-F]+)')!=p['cr2']:raise ValueError('S861U CR2')
        records.append((int(m[1]),rec))
    return records
