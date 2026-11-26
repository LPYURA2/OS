"""Independent UART + raw CPU proof of trusted stop and same-image relaunch."""
import re,time
from irq_trace_accept_859 import HEADER
from runtime_mixed_863_verify import row
from native_catalog_865_verify import complete_uart_lines

ADMIT=b'STYLEOS NATIVE ADMIT'
EVENT=b'STYLEOS NATIVE EVENT'
RETIRE=b'STYLEOS S865 FX-RETIRE'
FORCED=b'STYLEOS NATIVE FORCED-STOP'
RESTART=b'STYLEOS NATIVE STOP-RESTART'
DONE=b'STYLEOS NATIVE STOP-DONE'
REFUSE=b'STYLEOS NATIVE STOP-REFUSALS'
LEASE=b'STYLEOS NATIVE LEASE'
IRQ=b'STYLEOS NATIVE IRQ-RETIRE'
DRAIN=b'STYLEOS S865 TIMER-DRAIN'
AKEY=('slot','pid','gen','root','entry')
EKEY=('slot','pid','root','vector','error','rip','sp','flags','rax','state')
RKEY=('id','root','gen')
FKEY=('slot','pid','gen','root')
XKEY=('old-pid','old-gen','pid','gen','root','peer','peer-ticks')

def records(lines,tag,keys):
    return [(i,row(x,tag,keys)) for i,x in enumerate(lines) if x.startswith(tag+b' ')]

def wait_progress(proc,serial,predicate):
    end=time.monotonic()+60
    while time.monotonic()<end:
        raw=serial.read_bytes() if serial.exists() else b''
        if b' FAIL ' in raw or proc.poll() is not None:raise RuntimeError('forced stop guest failed')
        if predicate(complete_uart_lines(raw)):return
        time.sleep(.03)
    raise RuntimeError('forced stop progress timeout')

def ticks(lines,pid,after=-1):
    return sum(i>after and e['pid']==pid and e['vector']==0x50 for i,e in records(lines,EVENT,EKEY))

def wait_stop_ready(proc,serial):
    def ready(lines):
        retired=records(lines,RETIRE,RKEY)
        return len(retired)==30 and all(ticks(lines,pid,retired[-1][0])>=4 for pid in (1,2))
    wait_progress(proc,serial,ready)

def wait_restart_ready(proc,serial):
    def ready(lines):
        restarted=records(lines,RESTART,XKEY)
        if len(restarted)!=1:return False
        i,r=restarted[0]
        return ticks(lines,r['pid'],i)>=4 and ticks(lines,2,i)>=4
    wait_progress(proc,serial,ready)

def verify_forced_stop(lines,trace,obs):
    safe=obs['active']['root']
    def rec(tag,keys):return records(lines,tag,keys)
    admissions=rec(ADMIT,AKEY)
    batches=rec(b'STYLEOS NATIVE CATALOG',('valid','requested','accepted','resident'))
    events=rec(EVENT,EKEY);retired=rec(RETIRE,RKEY);stops=rec(FORCED,FKEY)
    leases=rec(LEASE,('pid','gen','pa','pages'))
    refusals=rec(REFUSE,('pid','gen'));restarts=rec(RESTART,XKEY)
    done=rec(DONE,('resident','dispatches'))
    if (len(admissions),len(batches),len(retired),len(stops),len(leases),len(refusals),len(restarts),len(done))!=(33,1,33,3,33,2,1,1):raise ValueError('forced stop evidence counts')
    if tuple(batches[0][1].values())!=(1,32,32,32) or not admissions[31][0]<batches[0][0]<events[0][0]:raise ValueError('initial residency')
    initial=[a for _,a in admissions[:32]]
    roots=[a['root'] for a in initial]
    for i,a in enumerate(initial):
        if tuple(a.values())!=(i,i+1,i+1,roots[i],0x10000000) or not roots[i] or roots[i]&4095:raise ValueError('initial identity')
    if len(set(roots))!=32 or safe in roots:raise ValueError('distinct roots')
    ai,fresh=admissions[-1]
    if tuple(fresh.values())!=(0,33,33,roots[0],0x10000000):raise ValueError('fresh PID/generation and reused root')
    by_pid={a['pid']:(i,a) for i,a in admissions}
    for (i,a),(j,l) in zip(admissions,leases):
        if i>=j or (l['pid'],l['gen'],l['pages'])!=(a['pid'],a['gen'],3) or not l['pa'] or l['pa']&4095:raise ValueError('lease identity')
    for k,(_,l) in enumerate(leases[:32]):
        if any(abs(l['pa']-x['pa'])<3*4096 for _,x in leases[:k]):raise ValueError('initial leases overlap')
    if leases[-1][1]['pa']!=leases[0][1]['pa']:raise ValueError('same physical lease not reused')
    r0,r1,r2=stops
    if [tuple(s.values()) for _,s in stops]!=[(0,1,1,roots[0]),(0,33,33,roots[0]),(1,2,2,roots[1])]:raise ValueError('forced stop identity')
    ri,restart=restarts[0]
    if (restart['old-pid'],restart['old-gen'],restart['pid'],restart['gen'],restart['root'],restart['peer'])!=(1,1,33,33,roots[0],2):raise ValueError('restart identity')
    if [tuple(r.values()) for _,r in refusals]!=[(1,1),(33,33)] or not refusals[0][0]<r0[0]<ai<ri<refusals[1][0]<r1[0]<r2[0]<done[0][0]:raise ValueError('stop and restart ordering')
    if tuple(done[0][1].values())!=(0,len(events)):raise ValueError('empty registry or dispatch accounting')
    if any(x.startswith(b'STYLEOS NATIVE EMPTY ') for x in lines):raise ValueError('unexpected empty pump')
    retire_by_gen={r['gen']:(i,r) for i,r in retired}
    if len(retire_by_gen)!=33:raise ValueError('duplicate retirement')
    stop_by_pid={s['pid']:i for i,s in stops}
    per={pid:[] for pid in by_pid}
    for i,e in events:
        pid=e['pid']
        if pid not in by_pid:raise ValueError('unknown PID')
        start,a=by_pid[pid];end,r=retire_by_gen[a['gen']]
        if not start<i<end or (e['slot'],e['root'],e['sp'])!=(a['slot'],a['root'],0x11001000) or e['error'] or e['flags']&0x202!=0x202:raise ValueError('event after retirement or wrong identity')
        per[pid].append((i,e))
    if [e['slot'] for _,e in events[:32]]!=list(range(32)):raise ValueError('first round fairness')
    for pid,(start,a) in by_pid.items():
        end,r=retire_by_gen[a['gen']]
        if (r['id'],r['root'])!=(a['slot'],a['root']) or not per[pid]:raise ValueError('retirement owner')
        seq=[e for _,e in per[pid]]
        if pid in (1,2,33):
            if (seq[0]['vector'],seq[0]['rip'],seq[0]['rax'],seq[0]['state'])!=(128,0x1000000d,1,1) or len(seq)<5 or any((e['vector'],e['rip'],e['rax'],e['state'])!=(0x50,0x1000000d,0,1) for e in seq[1:]):raise ValueError('real loop preemption')
            if not end<stop_by_pid[pid]:raise ValueError('release before successful stop')
        else:
            actual=[(e['vector'],e['rip'],e['rax'],e['state']) for e in seq]
            expected=[(128,0x1000000d,1,1),(6,0x1000000d,0,5) if a['slot']%8==7 else (128,0x10000014,2,4)]
            if actual!=expected or end>=r0[0]:raise ValueError('peer completion')
    last_normal=max(retire_by_gen[k][0] for k in range(3,33))
    if any(ticks(lines,pid,last_normal)<4 for pid in (1,2)) or ticks(lines,33,ri)<4 or sum(i>ri and i<r2[0] and e['vector']==0x50 for i,e in per[2])<4:raise ValueError('peer not independently preempted')
    if restart['peer-ticks']!=sum(i<r0[0] and e['vector']==0x50 for i,e in per[2]):raise ValueError('peer tick accounting')
    irqs=rec(IRQ,('slot','root','initial','current','lvt','isr','irr'))
    expected=[(i,e) for i,e in events if e['vector']==0x50]
    if len(irqs)!=len(expected):raise ValueError('IRQ count')
    previous=-1
    for (j,r),(i,e) in zip(irqs,expected):
        if tuple(r.values())!=(e['slot'],e['root'],0,0,0x10050,0,0) or not previous<j<i:raise ValueError('IRQ retirement evidence')
        previous=i
    # Actual CPU trace: all CPL3 and safe-root interrupts must be accounted for.
    headers=list(HEADER.finditer(trace));drains=rec(DRAIN,('id','safe','pc','vector'))
    idx=di=0;started=False;result=[]
    for j,m in enumerate(headers):
        block=trace[m.start():headers[j+1].start() if j+1<len(headers) else len(trace)]
        def one(pattern):
            values=re.findall(pattern,block,re.M)
            if len(values)!=1:raise ValueError('raw CPU field '+pattern)
            return int(values[0],16)
        cr3=one(r'\bCR3=([0-9a-fA-F]+)\b')
        if not started:
            if obs.get('boot_probes') is False:
                if cr3 not in [safe,*roots]:continue
                started=True
            else:
                if cr3!=safe:continue
                if tuple(int(x,16) for x in m.groups()[1:5])!=(3,0,1,0) or int(m[7],16)!=obs['bp']['rip']-1:raise ValueError('safe breakpoint')
                started=True;continue
        vec,err,soft,cpl,cs,rip,pc,ss,sp=map(lambda x:int(x,16),m.groups()[1:])
        if cpl==0:
            if di>=len(drains):raise ValueError('unaccounted safe interrupt')
            d=drains[di][1];di+=1
            if (vec,err,soft,cr3,rip,pc)!=(0x50,0,0,safe,d['pc'],d['pc']):raise ValueError('safe interrupt')
            continue
        if idx>=len(events):raise ValueError('unexpected CPU event after stop')
        e=events[idx][1];idx+=1
        actual=e['rip']-2*(e['vector']==128)
        if (vec,err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(e['vector'],e['error'],int(e['vector']==128),3,0x5b,actual,actual,0x63,e['sp'],e['root']):raise ValueError('raw CPU mismatch')
        if one(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b')!=e['flags'] or one(r'\bRAX=([0-9a-fA-F]+)\b')!=e['rax'] or one(r'\bR12=([0-9a-fA-F]+)\b')!=e['slot']+1:raise ValueError('raw CPU registers')
        result.append((int(m[1]),e))
    if not started or idx!=len(events) or di!=len(drains):raise ValueError('raw CPU count')
    fields=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')
    inputs=rec(b'STYLEOS S863 EVENT',fields)
    if sum(e['key']==0x43 for _,e in inputs)!=3 or len(inputs)<2:raise ValueError('three trusted request inputs')
    (i,prev),(j,lang)=inputs[-2:]
    if not done[0][0]<i<j or (prev['key'],lang['key'])!=(0x58,0x40) or lang['root']!=safe or lang['ru']==prev['ru'] or not lang['writes']:raise ValueError('responsive shell after forced stop')
    return result
