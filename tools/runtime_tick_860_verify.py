"""Bind registry events to the original S848/S849 proof; actual capture only in CI."""
import re
from fx_context_849_verify import verify as prior,verify_link_map
from user_preempt_848_verify import ORDER
POLICY=b'STYLEOS S860 POLICY actual-cpl3-ticks=yes fp=yes existing-dispatcher=yes leases-released=yes production-cr3=deny'
KEYS=('step','id','pid','handle','dispatch','safe','root','ticks','state','resident','window','minimized')
def parse(row,tag,keys):
    m=re.fullmatch(b'STYLEOS S860 '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),row)
    if not m:raise ValueError('S860 schema '+str(tag))
    return dict(zip(keys,(int(v,16) for v in m.groups())))
def verify(raw,rc):
    try:
        ok,obs=prior(raw,rc)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[x for x in lines if x.startswith(b'STYLEOS S860 ')]
        if len(rows)!=13 or rows[-1]!=POLICY:raise ValueError('S860 record count/policy')
        r=obs['child_roots'];safe=r['safe'];ready=parse(rows[0],b'READY',('safe','pid-a','pid-b','handle-a','handle-b','win-a','win-b'))
        # Bind handles to the actual S846 lease generation, not a synthetic PID.
        lease=next(x for x in lines if x.startswith(b'STYLEOS S846 READY '))
        handles=tuple(int(re.search(rb'\bgen-'+k+rb'=0x([0-9a-f]{16})',lease)[1],16) for k in (b'a',b'b'))
        if tuple(ready.values())!=(safe,1,2,*handles,1,2) or not all(handles) or handles[0]==handles[1]:raise ValueError('S860 admission')
        events=[x for x in lines if x.startswith((b'STYLEOS S848 TICK ',b'STYLEOS S848 END '))]
        backs=[x for x in lines if x.startswith(b'STYLEOS S848 BACK ')]
        if not lines.index(rows[0])<next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S848 READY ')):raise ValueError('S860 READY order')
        ticks=[0,0];records=[]
        for step,id in enumerate(ORDER):
            terminal=step in (6,9)
            if not terminal:ticks[id]+=1
            v=parse(rows[step+1],b'EVENT',KEYS)
            if tuple(v.values())!=(step,id,id+1,handles[id],step+1,safe,r['ab'[id]],ticks[id],(4 if id else 5) if terminal else 1,12288,0 if terminal else id+1,0 if terminal else int(id==0)):raise ValueError('S860 dispatcher event')
            if not lines.index(events[step])<lines.index(rows[step+1])<lines.index(backs[step]):raise ValueError('S860 safe return ordering')
            records.append(v)
        clean=parse(rows[-2],b'CLEAN',('safe','dispatches','processes','windows'))
        if tuple(clean.values())!=(safe,10,0,0) or ticks!=[3,5] or not lines.index(backs[-1])<next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S846 CLEAN '))<lines.index(rows[-2])<lines.index(POLICY):raise ValueError('S860 release order')
        obs.update(runtime_ticks=records,runtime_clean=clean,runtime_admission=ready);return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
HEADER=re.compile(r'^\s*(\d+): v=([0-9a-fA-F]{2}) e=([0-9a-fA-F]+) i=([01]) cpl=([0-3]) IP=([0-9a-fA-F]+):([0-9a-fA-F]+) pc=([0-9a-fA-F]+) SP=([0-9a-fA-F]+):([0-9a-fA-F]+)\b',re.M)
def verify_trace(text,obs):
    roots=obs['child_roots'];sel=obs['process_selectors'];expected=[];ti=ei=0
    # S847 precedes S848/849 on these child roots. INT80 saves the next RIP.
    for p in obs['process_traps']:
        soft=int(p['vector']==0x80);rip=p['rip']-2*soft
        expected.append((p['vector'],p['error'],soft,3,p['cs'],rip,rip,p['ss'],p['rsp'],p['root']))
    if len(expected)!=8:raise ValueError('S860 S847 event count')
    for step,id in enumerate(ORDER):
        if step in (6,9):
            p=obs['user_preempt_ends'][ei];ei+=1
            # INT80 trace IP is instruction start, UART trap frame points after it.
            rip=p['rip']-2 if id else p['rip'];expected.append((p['vector'],p['error'],int(id==1),3,sel['cs'],rip,rip,sel['ss'],0x11001000,roots['ab'[id]]))
        else:
            p=obs['user_preempt_ticks'][ti];ti+=1;expected.append((0x50,0,0,3,p['cs'],p['rip'],p['rip'],p['ss'],p['rsp'],p['root']))
    # The parent S843 CPL0 trial runs AFTER the CPL3 trials. Its UART has
    # already validated roots, sites, stacks, GPRs, EOI and bounded deferrals.
    # Keep every suffix event, including deferred IRQs in their original order.
    kernel=obs['ticks'];deferred=obs.get('deferred_irqs',[])
    if len(kernel)!=16 or len(deferred)>16 or obs['delivery']['hits']!=16+len(deferred):raise ValueError('S860 S843 count')
    tail=sorted([(p['n'],1,0,p) for p in kernel]+[(p['n'],0,p['count'],p) for p in deferred],key=lambda x:x[:3])
    for _,_,_,p in tail:
        # S783/S847 fixed owned GDT: kernel code 0x38, kernel data 0x30.
        expected.append((0x50,0,0,0,0x38,p['rip'],p['rip'],0x30,p['rsp'],p['root']))
    h=list(HEADER.finditer(text));events=[]
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)];rr=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if len(rr)!=1:raise ValueError('trace CR3')
        rec=tuple(int(v,16) for v in m.groups()[1:])+(int(rr[0],16),);events.append((int(m[1]),rec))
    # No unvalidated event may be inserted in or after the three trial phases.
    # No earlier event on a child root is allowed either.
    actual=events[-len(expected):]
    if any(v[-1] in (roots['a'],roots['b']) for _,v in events[:-len(expected)]):raise ValueError('S860 unexpected earlier child event')
    if [v for _,v in actual]!=expected or any(b[0]!=a[0]+1 for a,b in zip(actual,actual[1:])):raise ValueError('S860 CPU event sequence')
    return actual
