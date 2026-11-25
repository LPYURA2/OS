"""Mixed S863 guest: retain old root/framebuffer proof, bind all raw events."""
import re
from desktop_switch_851_site import bp_site
from fault_site import _unpack,_range
from runtime_roots_861_verify import verify as roots_verify,trace_check as roots_trace,HEADER
from user_preempt_848_verify import fixture_order
from runtime_tick_860_verify import KEYS
CODE=0x10000000
IST=0x400000003000
GPRS=('rax','rbx','rcx','rdx','rsi','rdi','rbp','r8','r9','r10','r11','r12','r13','r14','r15')
ORDER=[(0,'call'),(1,'call')]+[(i,'end' if t else 'tick') for i,t in fixture_order((5,3))]
def loop_site(efi,map_file):
    s=map_file.read_text();site=bp_site(efi,s);site['child_rva']=bp_site(efi,s,symbol='s861_bp_return')['rva']
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    def sym(name,obj,write=False):
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('mixed map '+name)
        seg,rel,va=map(lambda x:int(x,16),rows[0])
        if not 1<=seg<=count:raise ValueError('mixed segment')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if va-site['preferred_base']!=v+rel or not 0<=rel<max(vs,rs) or bool(flags&0x80000000)!=write or bool(flags&0x20000000)==write:raise ValueError('mixed symbol rights '+name)
        return v+rel,ro+rel
    site['timer_idt_rva'],_=sym('idt848',r'boot_x86_64_user_preemption_848\.o',True)
    site['common_event_rva'],_=sym('rt863_bank_event',r'kernel_runtime_bank_862\.o')
    for label,last,delta in [('a','fault',20),('b','exit',19)]:
        symbols={k:sym('s848_'+label+'_'+k,r'arch_x86_64_process_entry_847\.o') for k in ('start','yield','loop_end',last)}
        start=symbols['start'][0];y,yo=symbols['yield'];end=symbols['loop_end'][0];term,to=symbols[last]
        if y-start!=28 or end-start!=53 or term-start!=43+delta:raise ValueError('mixed linked offsets')
        if _range(efi,yo-2,2,'yield')!=b'\xcd\x80' or (label=='a' and _range(efi,to,3,'fault')!=b'\x48\x8b\x00') or (label=='b' and _range(efi,to-2,2,'exit')!=b'\xcd\x80'):raise ValueError('mixed linked instruction')
        site[label+'_yield']=CODE+y-start;site[label+'_end']=CODE+end-start;site[label+'_term']=CODE+term-start
    return site
def row(line,prefix,keys):
    m=re.fullmatch(prefix+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
    if not m:raise ValueError('mixed row '+str(prefix))
    return dict(zip(keys,(int(v,16) for v in m.groups())))
def verify(raw,rc,site,ref,*,epoch=0,clean_fnv=None,cold=False):
    try:
        ok,o=roots_verify(raw,rc,site,ref,generation_base=(1 if cold else 4)+2*epoch,clean_fnv=clean_fnv,cold=cold)
        offset=12*epoch
        identity=2*epoch
        if not ok:raise ValueError(o)
        lines=raw.splitlines()
        def rows(p):return [x for x in lines if x.startswith(p)]
        def exact(p): 
            v=rows(p)
            if len(v)!=1:raise ValueError('mixed count '+str(p))
            return v[0]
        def eq(v,w,tag):
            if tuple(v.values())!=tuple(w):raise ValueError('mixed '+tag)
        root=o['join_roots'];a=o['active'];safe=root['safe'];img=int(exact(b'STYLEOS MEMMAP IMAGE ').split()[-2],16)
        ready=row(exact(b'STYLEOS S861T READY '),b'STYLEOS S861T READY',('safe','idt','cs','ss','limit'))
        eq(ready,(safe,img+site['timer_idt_rva'],0x5b,0x63,0x67),'IDT')
        s848=row(exact(b'STYLEOS S848 READY '),b'STYLEOS S848 READY',('safe','a','b','lapic','cs','ss','a-loop-end','b-loop-end','a-fault','b-exit','count'))
        eq(s848,(safe,root['a'],root['b'],0xfee00000,0x5b,0x63,site['a_end'],site['b_end'],site['a_term'],site['b_term'],5000000),'sites')
        ad=row(exact(b'STYLEOS S848 ADMIT '),b'STYLEOS S848 ADMIT',('lvt','initial','current','div','isr','irr'))
        if not ad['lvt']&0x10000 or ad['lvt']&~0x300ff or ad['div']&~0xb or ad['current']>ad['initial'] or ad['isr'] or ad['irr']:raise ValueError('mixed admission')
        eq(row(exact(b'STYLEOS S848 STOP '),b'STYLEOS S848 STOP',('lvt','initial','current','isr','irr')),(ad['lvt'],0,0,0,0),'timer stop')
        frames=rows(b'STYLEOS S861T FRAME ');caps=rows(b'STYLEOS S849 CAPTURE ');enter=rows(b'STYLEOS S862B ENTER ');commit=rows(b'STYLEOS S862B COMMIT ');boundary=rows(b'STYLEOS S861T BOUNDARY ');ev=rows(b'STYLEOS S860 EVENT ');backs=rows(b'STYLEOS S848 BACK ')
        traps=rows(b'STYLEOS S863M CALL ')+[] # collect in raw order below
        traps=[x for x in lines if x.startswith((b'STYLEOS S863M CALL ',b'STYLEOS S848 TICK ',b'STYLEOS S848 END '))]
        resume=rows(b'STYLEOS S863M RESUME ')
        if any(len(x)!=12 for x in (frames,caps,enter,commit,boundary,ev,backs,traps)) or len(resume)!=2:raise ValueError('mixed event count')
        ticks=[0,0];seen=[0,0];prog=[0,0];saved=[(CODE,0x11001000,2)]*2;events=[];windows=2
        for step,(i,kind) in enumerate(ORDER):
            label='ab'[i];child=root[label];tag=0x8480000000000000|(i<<32);term=kind=='end'
            ft=row(frames[step],b'STYLEOS S861T FRAME',('step','flags'));flags=ft['flags']
            if ft['step']!=step or flags&0x202!=0x202 or flags&~0xad7:raise ValueError('mixed flags')
            eq(row(enter[step],b'STYLEOS S862B ENTER',('step','id','pid','dispatch','safe','root','pc','sp','flags')),(step,i,i+1+identity,step+1+offset,safe,child,saved[i][0],saved[i][1],saved[i][2]|0x200),'entry')
            seen[i]+=1
            eq(row(caps[step],b'STYLEOS S849 CAPTURE',('id','root','n','xmm14','xmm15','mxcsr','st0','exp','kernel-restored','terminal')),(i,child,seen[i],tag|13,tag|14,0x3f80 if i else 0x1f80,0 if i else 0x8000000000000000,0 if i else 0x3fff,1,int(term)),'FP')
            vec=128 if kind=='call' or term and i else 14 if term else 0x50
            err=5 if vec==14 else 0
            if kind=='call':
                v=row(traps[step],b'STYLEOS S863M CALL',('id','root','rip','rsp','flags','frame','rax','timer','isr','irr'))
                rip=site[label+'_yield']
                eq(v,(i,child,rip,0x11001000,flags,IST+4096-16-176,1,0,0,0),'call')
                eq(row(resume[i],b'STYLEOS S863M RESUME',('id','pc','rax','ticks')),(i,rip,0,0),'call result')
                if not lines.index(traps[step])<lines.index(resume[i])<lines.index(commit[step]):raise ValueError('mixed call order')
            elif kind=='tick':
                v=row(traps[step],b'STYLEOS S848 TICK',('n','id','root','rip','rsp','flags','frame','cs','ss','progress','isr','eoi')+GPRS)
                ticks[i]+=1;rip=v['rip']
                if rip not in (site[label+'_end']-10,site[label+'_end']-7,site[label+'_end']-2) or v['progress']<=prog[i] or v['isr']!=0x10000:raise ValueError('mixed IRQ progress')
                eq(v,(sum(ticks),i,child,rip,0x11001000,flags,IST+4096-16-176,0x5b,0x63,v['progress'],0x10000,1,*[0x12000000 if j==5 else tag|j for j in range(15)]),'IRQ frame')
                prog[i]=v['progress']
            else:
                rip=site[label+'_term'];windows-=1
                eq(row(traps[step],b'STYLEOS S848 END',('id','root','rip','vector','error','cr2','state','ticks')),(i,child,rip,vec,err,IST if err else 0,4 if i else 3,sum(ticks)),'terminal')
            state=(4 if i else 5) if term else 1
            saved[i]=(0,0,0) if term else (rip,0x11001000,flags&~0x200)
            eq(row(commit[step],b'STYLEOS S862B COMMIT',('step','id','pid','state','pc','sp','flags','zero')),(step,i,i+1+identity,state,*saved[i],int(term)),'commit')
            eq(row(boundary[step],b'STYLEOS S861T BOUNDARY',('step','safe','gdt','idt','pid')),(step,safe,a['gdt'],a['idt'],i+1+identity),'boundary')
            eq(row(ev[step],b'STYLEOS S860 EVENT',KEYS),(step,i,i+1+identity,root['gen-'+label],step+1+offset,safe,child,ticks[i],state,12288,0 if term else i+1+identity,0 if term else int(not i)),'registry')
            back=row(backs[step],b'STYLEOS S848 BACK',('step','id','safe','state','a','b','stack-same'))
            if back['ab'[1-i]]!=prog[1-i] or back[label]<prog[i] or not term and back[label]!=prog[i]:raise ValueError('mixed isolation')
            prog[i]=back[label]
            eq(back,(step,i,safe,4 if term and i else 3 if term else 1,*prog,1),'back')
            order=[enter[step],caps[step],frames[step],traps[step],commit[step],boundary[step],ev[step],backs[step]]
            if [lines.index(x) for x in order]!=sorted(lines.index(x) for x in order):raise ValueError('mixed ordering')
            events.append(dict(id=i,kind=kind,vector=vec,error=err,rip=rip,flags=flags,root=child))
        eq(row(exact(b'STYLEOS S848 TOTAL '),b'STYLEOS S848 TOTAL',('ticks','a','b','config-restored','timer-stopped')),(8,5,3,1,1),'timer cleanup')
        eq(row(exact(b'STYLEOS S861T SCRUB '),b'STYLEOS S861T SCRUB',('safe','used','tables-zero','leases-zero')),(safe,0,1,1),'scrub')
        eq(row(exact(b'STYLEOS S860 CLEAN '),b'STYLEOS S860 CLEAN',('safe','dispatches','processes','windows')),(safe,12+offset,0,0),'clean')
        eq(row(exact(b'STYLEOS S862B CLEAN '),b'STYLEOS S862B CLEAN',('safe','bank-zero','pending-zero')),(safe,1,1),'bank cleanup')
        eq(row(exact(b'STYLEOS S862L READY '),b'STYLEOS S862L READY',('a','b','watchdog')),(5,3,32),'loop plan')
        eq(row(exact(b'STYLEOS S862L EMPTY '),b'STYLEOS S862L EMPTY',('dispatches','safe','windows')),(12,safe,0),'loop empty')
        if not lines.index(exact(b'STYLEOS S862L READY '))<lines.index(enter[0])<lines.index(backs[-1])<lines.index(exact(b'STYLEOS S862L EMPTY '))<lines.index(exact(b'STYLEOS S849 CLEAN ')):raise ValueError('mixed loop order')
        if exact(b'STYLEOS S849 CLEAN ')!=b'STYLEOS S849 CLEAN slots-zero=yes kernel-copy-zero=yes' or ticks!=[5,3] or seen!=[7,5] or windows:raise ValueError('mixed final')
        o.update(mixed_events=events,timer_ready=ready);return True,o
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
def trace_check(text,o,*,initial_bp=True):
    h=list(HEADER.finditer(text));root=o['join_roots'];start=None
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)]
        if any(int(x,16) in (root['safe'],root['a'],root['b']) for x in re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)):
            start=i;break
    prefix=3 if initial_bp else 2
    if start is None or len(h)-start!=prefix+12:raise ValueError('mixed CPU count')
    out=roots_trace(text[:h[start+prefix].start()],o,initial_bp=initial_bp)
    for j,e in enumerate(o['mixed_events']):
        k=start+prefix+j;m=h[k];block=text[m.start():h[k+1].start() if k+1<len(h) else len(text)]
        def val(p):
            v=re.findall(p,block,re.M)
            if len(v)!=1:raise ValueError('mixed CPU field '+p)
            return int(v[0],16)
        soft=int(e['vector']==128);rip=e['rip']-2*soft;a=o['active']
        rec=tuple(int(v,16) for v in m.groups()[1:])+(val(r'\bCR3=([0-9a-fA-F]+)'),val(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)'),val(r'^RIP=([0-9a-fA-F]+)'),val(r'\bRSP=([0-9a-fA-F]+)'),val(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^TR\s*=([0-9a-fA-F]+)\b'))
        want=(e['vector'],e['error'],soft,3,0x5b,rip,rip,0x63,0x11001000,e['root'],e['flags'],rip,0x11001000,a['gdt'],o['timer_ready']['idt'],a['tr'])
        if rec!=want or int(m[1])!=out[-1][0]+1:raise ValueError('mixed CPU binding')
        if e['error'] and val(r'^CR0=[^\n]*\bCR2=([0-9a-fA-F]+)')!=IST:raise ValueError('mixed CR2')
        for t,name in enumerate(GPRS):
            expected=0x12000000 if t==5 else 0x8480000000000000|(e['id']<<32)|t
            if t==0 and e['kind']!='tick':expected=1 if e['kind']=='call' else 2 if e['id'] else IST
            if val(r'\b'+name.upper()+r'\s*=([0-9a-fA-F]+)\b')!=expected:raise ValueError('mixed CPU GPR '+name)
        out.append((int(m[1]),rec))
    return out
