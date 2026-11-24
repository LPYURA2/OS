"""Strict transcript; preserve prior genuine S847/S846/S843 evidence."""
import re
from process_trial_847_verify import verify as prior, verify_link_map as prior_map
GPRS=('rax','rbx','rcx','rdx','rsi','rdi','rbp','r8','r9','r10','r11','r12','r13','r14','r15')
CODE=0x10000000
IST=0x400000003000
ORDER=(0,1,0,1,0,1,0,1,1,1)
POLICY=b'STYLEOS S848 POLICY cpl3-timer=yes integer-only=yes single-cpu=yes firmware-timer-phase=discarded production-cr3=deny'

def parse(line,tag,keys):
    prefix=('STYLEOS S848 '+tag+' ').encode()
    if not line.startswith(prefix):raise ValueError('S848 order '+tag)
    match=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
    if not match:raise ValueError('S848 schema '+tag)
    return dict(zip(keys,(int(v,16) for v in match.groups())))

def verify_stage(lines,obs):
    rec=[x for x in lines if x.startswith(b'STYLEOS S848 ')]
    if len(rec)!=25 or rec[-1]!=POLICY:raise ValueError('S848 record count/policy')
    parent=obs['child_roots'];safe=parent['safe'];sel=obs['process_selectors']
    admit=parse(rec[0],'ADMIT',('lvt','initial','current','div','isr','irr'))
    if any(v>0xffffffff for v in admit.values()) or not admit['lvt']&0x10000 or admit['lvt']&~0x300ff or admit['div']&~0xb or admit['current']>admit['initial'] or admit['isr'] or admit['irr']:raise ValueError('S848 timer admission')
    stop=parse(rec[1],'STOP',('lvt','initial','current','isr','irr'))
    if tuple(stop.values())!=(admit['lvt'],0,0,0,0):raise ValueError('S848 timer quiescence')
    r=parse(rec[2],'READY',('safe','a','b','lapic','cs','ss','a-loop-end','b-loop-end','a-fault','b-exit','count'))
    if tuple(r.values())!=(safe,parent['a'],parent['b'],0xfee00000,sel['cs'],sel['ss'],CODE+10,CODE+10,CODE+20,CODE+19,5000000):raise ValueError('S848 roots/sites')
    i=3;ticks=0;local=[0,0];progress=[0,0];seen=[];backs=[];ends=[]
    for step,id in enumerate(ORDER):
        terminal=step in (6,9)
        if terminal:
            v=parse(rec[i],'END',('id','root','rip','vector','error','cr2','state','ticks'))
            if tuple(v.values())!=(id,parent['ab'[id]],CODE+(19 if id else 20),128 if id else 14,0 if id else 5,0 if id else IST,4 if id else 3,ticks):raise ValueError('S848 terminal evidence')
            ends.append(v)
        else:
            v=parse(rec[i],'TICK',('n','id','root','rip','rsp','flags','frame','cs','ss','progress','isr','eoi')+GPRS)
            ticks+=1;local[id]+=1
            if (v['n'],v['id'],v['root'],v['rsp'],v['frame'],v['cs'],v['ss'],v['eoi'])!=(ticks,id,parent['ab'[id]],0x11001000,IST+4096-16-176,sel['cs'],sel['ss'],1):raise ValueError('S848 hardware context')
            if v['rip'] not in (CODE,CODE+3,CODE+8) or v['flags']&0x202!=0x202 or v['flags']&~0xad7 or not v['isr']&0x10000 or v['progress']<=progress[id]:raise ValueError('S848 progress/IRQ')
            for j,k in enumerate(GPRS):
                if v[k]!=(0x12000000 if j==5 else 0x8480000000000000|(id<<32)|j):raise ValueError('S848 GPR '+k)
            progress[id]=v['progress'];seen.append(v)
        i+=1
        b=parse(rec[i],'BACK',('step','id','safe','state','a','b','stack-same'));i+=1
        if (b['step'],b['id'],b['safe'],b['state'],b['stack-same'])!=(step,id,safe,(4 if id else 3) if terminal else 1,1):raise ValueError('S848 return')
        if b['ab'[1-id]]!=progress[1-id] or b['ab'[id]]<progress[id] or (not terminal and b['ab'[id]]!=progress[id]):raise ValueError('S848 cross-process write')
        progress[id]=b['ab'[id]];backs.append(b)
    total=parse(rec[i],'TOTAL',('ticks','a','b','config-restored','timer-stopped'));i+=1
    if i!=24 or tuple(total.values())!=(8,3,5,1,1) or ticks!=8 or local!=[3,5]:raise ValueError('S848 totals')
    before=next(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S847 POLICY '))
    after=next(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S846 CLEAN '))
    indices=[before]+[lines.index(x) for x in rec]+[after]
    if any(x>=y for x,y in zip(indices,indices[1:])):raise ValueError('S848 phase order')
    return dict(user_preempt_verified=True,user_preempt_admission=admit,user_preempt_stop=stop,user_preempt_ready=r,user_preempt_ticks=seen,user_preempt_returns=backs,user_preempt_ends=ends,user_preempt_total=total)

def verify(raw,rc):
    try:
        ok,obs=prior(raw,rc)
        if not ok:raise ValueError(obs)
        obs.update(verify_stage(raw.splitlines(),obs))
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)

def verify_link_map(path):
    prior_map(path)
    symbols={}
    for line in path.read_text().splitlines():
        m=re.search(r'\bs848_([ab])_(start|loop_end|fault|exit|end)\s+([0-9a-fA-F]{16})\b',line)
        if m:symbols[m[1]+'-'+m[2]]=int(m[3],16)
    for id,term,offset,size in [('a','fault',20,25),('b','exit',19,21)]:
        for site,want in [('loop_end',10),(term,offset),('end',size)]:
            if symbols[id+'-'+site]-symbols[id+'-start']!=want:raise ValueError('S848 link map '+id+'-'+site)
