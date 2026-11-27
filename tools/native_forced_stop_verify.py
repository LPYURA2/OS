"""Independent UART + raw CPU proof of trusted stop and same-image relaunch."""
import re,time
from irq_trace_accept_859 import HEADER
from runtime_mixed_863_verify import row
from native_catalog_865_verify import complete_uart_lines,FOCUS_RETURNS,FOCUS_LOOP,FRAME_RETURNS,FRAME_LOOP

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
        if b' FAIL ' in raw or b'STYLEOS DESKTOP RUNTIME BOUNDARY ERROR' in raw or proc.poll() is not None:raise RuntimeError('forced stop guest failed; inspect UART')
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

def wait_shell_boundary(proc,serial):
    def ready(lines):
        marks=[i for i,x in enumerate(lines) if x==b'STYLEOS DESKTOP SHELL RESTART']
        if len(marks)!=1:return False
        return all(ticks(lines,pid,marks[0])>=4 for pid in (1,2))
    wait_progress(proc,serial,ready)

def wait_focus_sequence(proc,serial,seq):
    def ready(lines):
        rows=records(lines,b'STYLEOS NATIVE INPUT',('pid','gen','root','window','va','bytes','status','seq'))
        return any(r['pid']==2 and r['status']==0 and r['seq']==seq for _,r in rows)
    wait_progress(proc,serial,ready)

def verify_shell_boundary(lines,roots,loop=0x1000000d,focus=False,frame=False):
    # Verify ordered actual guest receipts. The enclosing forced-stop
    # verifier also binds every EVENT to a CPL3 raw QEMU trap and R12.
    marks=[i for i,x in enumerate(lines) if x==b'STYLEOS DESKTOP SHELL RESTART']
    if len(marks)!=1:raise ValueError('exact live refresh marker')
    at=marks[0]
    stops=[(i,x) for i,x in records(lines,FORCED,FKEY) if not frame or x['pid'] not in (35,36,37)]
    retired=records(lines,RETIRE,RKEY)
    if len(stops)!=3 or len(retired)!=(38 if frame else 33):raise ValueError('boundary lifecycle')
    first_stop=stops[0][0]
    if not retired[29][0]<at<first_stop:raise ValueError('refresh must occur while both original peers alive')
    for pid in (1,2):
        events=[(i,e) for i,e in records(lines,EVENT,EKEY) if e['pid']==pid and e['vector']==0x50]
        before=[e for i,e in events if retired[29][0]<i<at]
        after=[e for i,e in events if at<i<first_stop]
        if len(before)<4 or len(after)<4:raise ValueError('insufficient execution across shell refresh')
        if any(e['root']!=roots[pid-1] or e['rip']!=(0x10000000+(FRAME_LOOP if frame else FOCUS_LOOP) if focus and pid==2 else loop) or e['state']!=1 for e in before+after):
            raise ValueError('native identity/state changed across refresh')
    fields=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')
    inputs=records(lines,b'STYLEOS S863 EVENT',fields)
    f10=[(i,e) for i,e in inputs if e['key']==0x44 and not e['mouse']]
    if len(f10)!=1:raise ValueError('actual F10 event missing')
    fi,fe=f10[0]
    earlier=[e for i,e in inputs if i<min(at,fi)]
    if not earlier:raise ValueError('missing pre-refresh state')
    if frame:
        later=[e for i,e in inputs if i>at]
        if not fi<at<first_stop or fe['restarts']!=earlier[-1]['restarts'] or not later or later[0]['restarts']!=fe['restarts']+1:
            raise ValueError('controller deferred refresh counter')
    elif not at<fi<first_stop or fe['restarts']!=earlier[-1]['restarts']+1:
        raise ValueError('legacy refresh counter')
    if any(fe[k]!=earlier[-1][k] for k in ('text','text-hash','text-dirty','note-session')):
        raise ValueError('shell document continuity')
    if frame:
        begin=records(lines,b'STYLEOS NATIVE SHELL-GAP-BEGIN',('dispatches',))
        end=records(lines,b'STYLEOS NATIVE SHELL-GAP-END',('dispatches',))
        if len(begin)!=1 or len(end)!=1:raise ValueError('supervisor gap evidence')
        lo,start=begin[0];hi,last=end[0]
        if not f10[0][0]<lo<hi<at<first_stop or last['dispatches']-start['dispatches']!=16:
            raise ValueError('supervisor gap dispatch count/order')
        gap=records(lines[lo+1:hi],EVENT,EKEY)
        if len(gap)!=16 or any(e['pid'] not in (1,2) or e['vector']!=0x50 or e['state']!=1 for _,e in gap):
            raise ValueError('actual application execution during shell gap')
        if any(sum(e['pid']==pid for _,e in gap)!=8 for pid in (1,2)):
            raise ValueError('fairness without shell callback')
        if any(lo<i<hi for i,_ in inputs):
            raise ValueError('desktop input observed while shell callback suspended')
    # Windows/ports ownership is separately verified by enclosing lifecycle
    # checks at bind, revoke, rebind and final cleanup. No pixel IPC claim.

def verify_rollback_probe(lines):
    begin=records(lines,b'STYLEOS NATIVE ROLLBACK-BEGIN',('dispatches',))
    end=records(lines,b'STYLEOS NATIVE ROLLBACK-END',('dispatches',))
    cap=records(lines,b'STYLEOS NATIVE WINDOW-LAUNCH-CAPACITY',('resident',))
    if len(begin)!=1 or len(end)!=1 or len(cap)!=1 or cap[0][1]['resident']!=32:
        raise ValueError('rollback or capacity evidence count')
    lo,start=begin[0];hi,last=end[0]
    if not cap[0][0]<lo<hi or start!=last:raise ValueError('rollback interval')
    part=lines[lo+1:hi]
    phases=records(part,b'STYLEOS NATIVE ROLLBACK-PHASE',('phase','used','resident','windows','ports','dispatches'))
    admits=records(part,ADMIT,AKEY);leases=records(part,LEASE,('pid','gen','pa','pages'))
    stops=records(part,FORCED,FKEY);retires=records(part,RETIRE,RKEY)
    if any(len(x)!=4 for x in (phases,admits,leases,stops,retires)):
        raise ValueError('rollback lifecycle counts')
    if records(part,EVENT,EKEY) or records(part,IRQ,('slot','root','initial','current','lvt','isr','irr')):
        raise ValueError('unpublished rollback process dispatched')
    for j,((ai,a),(li,l),(ri,r),(si,s),(pi,phase)) in enumerate(zip(admits,leases,retires,stops,phases),1):
        pid=38+j
        if tuple(a.values())!=(0,pid,pid,admits[0][1]['root'],0x10000000) or tuple(l.values())!=(pid,pid,leases[0][1]['pa'],3):
            raise ValueError('rollback fresh identity and reused lease')
        if tuple(r.values())!=(0,a['root'],pid) or tuple(s.values())!=(0,pid,pid,a['root']):
            raise ValueError('rollback retirement identity')
        if tuple(phase.values())!=(j,0,0,0,0,start['dispatches']) or not ai<li<ri<si<pi:
            raise ValueError('rollback resources or ordering')
        if j>1 and not phases[j-2][0]<ai:raise ValueError('rollback interleaving')
    original=lines[:lo]+lines[hi+1:]
    done=records(original,DONE,('resident','dispatches'))
    if len(done)!=1 or done[0][1]['dispatches']!=start['dispatches']:
        raise ValueError('rollback dispatch accounting')
    return original

def verify_forced_stop(lines,trace,obs,*,shell_boundary=False,window_ipc=False,window_tile=False,window_input=False,window_focus=False,window_frame=False):
    if window_frame:lines=verify_rollback_probe(lines)
    fresh_pid=38 if window_frame else 33
    safe=obs['active']['root']
    def rec(tag,keys):return records(lines,tag,keys)
    admissions=rec(ADMIT,AKEY)
    batches=rec(b'STYLEOS NATIVE CATALOG',('valid','requested','accepted','resident'))
    events=rec(EVENT,EKEY);retired=rec(RETIRE,RKEY);all_stops=rec(FORCED,FKEY)
    time_stops=[(i,x) for i,x in all_stops if window_frame and x['pid'] in (35,36,37)]
    stops=[(i,x) for i,x in all_stops if (i,x) not in time_stops]
    leases=rec(LEASE,('pid','gen','pa','pages'))
    refusals=rec(REFUSE,('pid','gen'));restarts=rec(RESTART,XKEY)
    done=rec(DONE,('resident','dispatches'))
    if (len(admissions),len(batches),len(retired),len(stops),len(leases),len(refusals),len(restarts),len(done))!=(fresh_pid,1,fresh_pid,3,fresh_pid,2,1,1):raise ValueError('forced stop evidence counts')
    if tuple(batches[0][1].values())!=(1,32,32,32) or not admissions[31][0]<batches[0][0]<events[0][0]:raise ValueError('initial residency')
    initial=[a for _,a in admissions[:32]]
    roots=[a['root'] for a in initial]
    for i,a in enumerate(initial):
        if tuple(a.values())!=(i,i+1,i+1,roots[i],0x10000000) or not roots[i] or roots[i]&4095:raise ValueError('initial identity')
    if len(set(roots))!=32 or safe in roots:raise ValueError('distinct roots')
    ai,fresh=admissions[-1]
    if tuple(fresh.values())!=(0,fresh_pid,fresh_pid,roots[0],0x10000000):raise ValueError('fresh PID/generation and reused root')
    if window_frame and [tuple(a.values()) for _,a in admissions[32:37]]!=[(2,pid,pid,roots[2],0x10000000) for pid in range(33,38)]:
        raise ValueError('fresh controller identity and same slot')
    by_pid={a['pid']:(i,a) for i,a in admissions}
    for (i,a),(j,l) in zip(admissions,leases):
        if i>=j or (l['pid'],l['gen'],l['pages'])!=(a['pid'],a['gen'],3) or not l['pa'] or l['pa']&4095:raise ValueError('lease identity')
    for k,(_,l) in enumerate(leases[:32]):
        if any(abs(l['pa']-x['pa'])<3*4096 for _,x in leases[:k]):raise ValueError('initial leases overlap')
    if leases[-1][1]['pa']!=leases[0][1]['pa']:raise ValueError('same physical lease not reused')
    r0,r1,r2=stops
    if [tuple(s.values()) for _,s in stops]!=[(0,1,1,roots[0]),(0,fresh_pid,fresh_pid,roots[0]),(1,2,2,roots[1])]:raise ValueError('forced stop identity')
    ri,restart=restarts[0]
    if (restart['old-pid'],restart['old-gen'],restart['pid'],restart['gen'],restart['root'],restart['peer'])!=(1,1,fresh_pid,fresh_pid,roots[0],2):raise ValueError('restart identity')
    if [tuple(r.values()) for _,r in refusals]!=[(1,1),(fresh_pid,fresh_pid)] or not refusals[0][0]<r0[0]<ai<ri<refusals[1][0]<r1[0]<r2[0]<done[0][0]:raise ValueError('stop and restart ordering')
    if tuple(done[0][1].values())!=(0,len(events)):raise ValueError('empty registry or dispatch accounting')
    bound=rec(b'STYLEOS NATIVE STOP-OWNERS-BOUND',('pid','window','peer','peer-window','ports'))
    revoked=rec(b'STYLEOS NATIVE STOP-OWNERS-REVOKED',('pid','window','peer','ports'))
    rebound=rec(b'STYLEOS NATIVE STOP-OWNERS-REBOUND',('pid','window','old-window','ports'))
    emptied=rec(b'STYLEOS NATIVE STOP-OWNERS-EMPTY',('pid','window','ports'))
    if (len(bound),len(revoked),len(rebound),len(emptied))!=(1,2,1,1):raise ValueError('ownership evidence count')
    bi,b=bound[0];vi,v=revoked[0];wi,w=revoked[1];ni,n=rebound[0];zi,z=emptied[0]
    if (b['pid'],b['peer'],b['ports'])!=(1,2,2) or not b['window'] or not b['peer-window'] or b['window']==b['peer-window']:raise ValueError('initial ownership')
    if tuple(v.values())!=(1,b['window'],2,1) or (n['pid'],n['old-window'],n['ports'])!=(fresh_pid,b['window'],2) or not n['window'] or n['window'] in (b['window'],b['peer-window']):raise ValueError('revoked and fresh ownership')
    if tuple(w.values())!=(fresh_pid,n['window'],2,1) or tuple(z.values())!=(2,b['peer-window'],0):raise ValueError('final ownership')
    if not batches[0][0]<bi<events[0][0] or not r0[0]<vi<ai<ni<ri<r1[0]<wi<r2[0]<zi<done[0][0]:raise ValueError('ownership lifecycle order')
    if any(x.startswith(b'STYLEOS NATIVE EMPTY ') for x in lines):raise ValueError('unexpected empty pump')
    retire_by_gen={r['gen']:(i,r) for i,r in retired}
    if len(retire_by_gen)!=fresh_pid:raise ValueError('duplicate retirement')
    stop_by_pid={s['pid']:i for i,s in all_stops}
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
        if pid in (1,2,fresh_pid):
            prefix=[(128,0x1000000d,1,1)]
            if window_tile:prefix.extend([(128,0x10000043,0x101,1),(128,0x1000005e,0x101,1)])
            elif window_ipc:prefix.append((128,0x1000001e,0x100,1))
            if window_input:prefix.extend([(128,0x1000007e,0x102,1),(128,0x1000009d,0x102,1)])
            loop=0x100000df if window_input else 0x10000064 if window_tile else 0x10000024 if window_ipc else 0x1000000d
            actual=[(e['vector'],e['rip'],e['rax'],e['state']) for e in seq]
            if actual[:len(prefix)]!=prefix:raise ValueError('initial native calls')
            tail=actual[len(prefix):]
            if window_focus and pid==2:
                # Four stages, one or more polls per stage. Each group ends
                # in the successfully returned event verified below.
                for off in FOCUS_RETURNS:
                    poll_count=0
                    while tail and tail[0]==(128,0x10000000+off,0x102,1):
                        tail=tail[1:];poll_count+=1
                    if not poll_count:raise ValueError('missing focus poll stage')
                loop=0x10000000+FOCUS_LOOP
                if window_frame:
                    expected=[(128,0x10000000+off,0x103,1) for off in FRAME_RETURNS]
                    if tail[:3]!=expected:raise ValueError('missing frame syscalls')
                    tail=tail[3:]
                    loop=0x10000000+FRAME_LOOP
            if len(tail)<4 or any(x!=(0x50,loop,0,1) for x in tail):raise ValueError('real loop preemption')
            if not end<stop_by_pid[pid]:raise ValueError('release before successful stop')
        else:
            actual=[(e['vector'],e['rip'],e['rax'],e['state']) for e in seq]
            if window_frame and pid in (3,33,34,35,36,37):
                expected=[(128,0x1000000d,1,1),(128,0x10000014,0x105,1)]
                if pid==33:expected.append((6,0x10000047,0x100000044,5))
                elif pid==34:expected.extend([(128,0x1000003b,0x106,1),(128,0x10000047,2,4)])
                elif pid==3:expected.append((128,0x10000047,2,4))
                else:
                    if actual[:2]!=expected or len(actual)<10 or any(x!=(0x50,0x10000049,((pid-32)<<32)|0x44,1) for x in actual[2:]) or not end<stop_by_pid[pid]<r0[0]:
                        raise ValueError('actual hung controller preemption and retirement')
                    expected=actual
            else:expected=[(128,0x1000000d,1,1),(6,0x1000000d,0,5) if a['slot']%8==7 else (128,0x10000014,2,4)]
            if actual!=expected or end>=r0[0]:raise ValueError('peer completion')
    last_normal=max(retire_by_gen[k][0] for k in range(3,33))
    if any(ticks(lines,pid,last_normal)<4 for pid in (1,2)) or ticks(lines,fresh_pid,ri)<4 or sum(i>ri and i<r2[0] and e['vector']==0x50 for i,e in per[2])<4:raise ValueError('peer not independently preempted')
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
        if window_ipc and e['vector']==128 and e['rax']==0x100:
            if one(r'\bRBX=([0-9a-fA-F]+)\b')!=0 or one(r'\bRCX=([0-9a-fA-F]+)\b')!=0xb10000+e['slot']+1:raise ValueError('raw pixel arguments')
        if window_tile and e['vector']==128 and e['rax']==0x101:
            want=0x12000ff8 if e['rip']==0x10000043 else 0x12000000
            if one(r'\bRBX=([0-9a-fA-F]+)\b')!=0 or one(r'\bRCX=([0-9a-fA-F]+)\b')!=want or one(r'\bRDX=([0-9a-fA-F]+)\b')!=0x20002:raise ValueError('raw tile arguments')
        if window_input and e['vector']==128 and e['rax']==0x102:
            want=0x12000fe0 if e['rip']==0x1000007e else 0x12000100
            if one(r'\bRBX=([0-9a-fA-F]+)\b')!=0 or one(r'\bRCX=([0-9a-fA-F]+)\b')!=want or one(r'\bRDX=([0-9a-fA-F]+)\b')!=64:raise ValueError('raw input arguments')
        if window_frame and e['vector']==128 and e['rax']==0x106:
            if e['pid']!=34 or one(r'\bRBX=([0-9a-fA-F]+)\b')!=2 or one(r'\bRCX=([0-9a-fA-F]+)\b')!=1 or one(r'\bRDX=([0-9a-fA-F]+)\b')!=0:
                raise ValueError('raw controller decision')
        if window_frame and e['vector']==128 and e['rax']==0x103:
            want=0x12000ff0 if e['rip']==0x10000000+FRAME_RETURNS[0] else 0x12000200
            if e['pid']!=2 or one(r'\bRBX=([0-9a-fA-F]+)\b')!=0 or one(r'\bRCX=([0-9a-fA-F]+)\b')!=want or one(r'\bRDX=([0-9a-fA-F]+)\b')!=2:raise ValueError('raw frame arguments')
        result.append((int(m[1]),e))
    if not started or idx!=len(events) or di!=len(drains):raise ValueError('raw CPU count')
    fields=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')
    inputs=rec(b'STYLEOS S863 EVENT',fields)
    if sum(e['key']==0x43 for _,e in inputs)!=3 or len(inputs)<2:raise ValueError('three trusted request inputs')
    (i,prev),(j,lang)=inputs[-2:]
    if not done[0][0]<i<j or (prev['key'],lang['key'])!=(0x58,0x40) or lang['root']!=safe or lang['ru']==prev['ru'] or not lang['writes']:raise ValueError('responsive shell after forced stop')
    if window_tile:
        tiles=rec(b'STYLEOS NATIVE TILE',('pid','gen','root','window','va','shape','status','first'))
        expected=[]
        for pid,idx in ((1,0),(2,1),(fresh_pid,0)):
            expected.extend([(pid,pid,roots[idx],0,0x12000ff8,0x20002,1,0),
                             (pid,pid,roots[idx],0,0x12000000,0x20002,0,0xc10001+idx)])
        # Scheduler interleaves the initial two peers' calls. Verify each
        # caller's exact ordered receipts, then match global trap order.
        if len(tiles)!=6:raise ValueError('tile receipt count')
        for pid in (1,2,fresh_pid):
            if [tuple(x.values()) for _,x in tiles if x['pid']==pid]!=[x for x in expected if x[0]==pid]:
                raise ValueError('tile acceptance or cross-page refusal')
        calls=[(i,e) for i,e in events if e['vector']==128 and e['rax']==0x101]
        if len(calls)!=6 or any(j+1!=i or x['pid']!=e['pid'] for (j,x),(i,e) in zip(tiles,calls)):
            raise ValueError('tile receipt not adjacent to authenticated trap')
    elif window_ipc:
        pixels=rec(b'STYLEOS NATIVE PIXEL',('pid','gen','root','window','rgb','status'))
        if len(pixels)!=3 or [tuple(x.values()) for _,x in pixels]!=[(1,1,roots[0],0,0xb10001,0),(2,2,roots[1],0,0xb10002,0),(fresh_pid,fresh_pid,roots[0],0,0xb10001,0)]:
            raise ValueError('authenticated pixel receipts')
        calls=[(i,e) for i,e in events if e['vector']==128 and e['rax']==0x100]
        if len(calls)!=3 or any(j+1!=i or x['pid']!=e['pid'] for (j,x),(i,e) in zip(pixels,calls)):
            raise ValueError('pixel receipt not adjacent to authenticated trap')
    if window_input:
        receipts=rec(b'STYLEOS NATIVE INPUT',('pid','gen','root','window','va','bytes','status','seq'))
        for pid,idx in ((1,0),(2,1),(fresh_pid,0)):
            want=[(pid,pid,roots[idx],0,0x12000fe0,64,1,0),(pid,pid,roots[idx],0,0x12000100,64,0,1)]
            got=[tuple(x.values()) for _,x in receipts if x['pid']==pid]
            if got[:2]!=want:raise ValueError('input destination preflight or event consumption')
            if window_focus and pid==2:
                later=got[2:]
                for seqno in (2,3,4,5):
                    while later and later[0]==(2,2,roots[1],0,0x12000100,64,6,0):later=later[1:]
                    if not later or later[0]!=(2,2,roots[1],0,0x12000100,64,0,seqno):
                        raise ValueError('focus polling receipt')
                    later=later[1:]
                if later:raise ValueError('extra focus delivery')
            elif got[2:]:raise ValueError('unexpected input to nonfocused peer')
        calls=[(i,e) for i,e in events if e['vector']==128 and e['rax']==0x102]
        if len(calls)!=len(receipts) or any(j+1!=i or x['pid']!=e['pid'] for (j,x),(i,e) in zip(receipts,calls)):
            raise ValueError('input receipt not adjacent to authenticated trap')
        if window_focus:
            focus_ok=[(i,x) for i,x in receipts if x['pid']==2 and x['seq'] in (3,4,5)]
            if len(focus_ok)!=3:raise ValueError('real input event count')
            marks=[i for i,x in enumerate(lines) if x==b'STYLEOS DESKTOP SHELL RESTART']
            if len(marks)!=1:raise ValueError('missing focus refresh')
            f9=[(i,e) for i,e in inputs if e['key']==0x43 and not e['mouse']]
            route=[(i,e) for i,e in inputs if f9[0][0]<i<marks[0]]
            down=[(i,e) for i,e in route if e['mouse'] and e['buttons']==1 and (e['x'],e['y'])==(1,1)]
            up=[(i,e) for i,e in route if e['mouse'] and not e['buttons'] and (e['x'],e['y'])==(1,1)]
            key=[(i,e) for i,e in route if not e['mouse'] and e['key']==0x1e]
            # Observer receipt may precede syscall by scheduler dispatch;
            # exact order and full user wire checks, no fabricated QMP data.
            if not down or not up or len(key)!=1 or not down[-1][0]<focus_ok[0][0]<up[-1][0]<focus_ok[1][0]<key[0][0]<focus_ok[2][0]<marks[0]:
                raise ValueError('real pointer/key input ordering')
            baseline=f9[0][1]
            if any(e[k]!=baseline[k] for _,e in route for k in ('text','text-hash','text-dirty','note-session')):
                raise ValueError('native focus leaked into legacy editor')
    if window_frame:
        controllers=rec(b'STYLEOS NATIVE SHELL-CONTROLLER',('pid','gen','root','epoch'))
        completed=rec(b'STYLEOS NATIVE SHELL-CONTROLLER-RETIRED',('pid','epoch','resident'))
        recovery=rec(b'STYLEOS NATIVE SHELL-RECOVERY',('pid','attempt','dispatches'))
        if len(recovery)!=5 or [(x['pid'],x['attempt']) for _,x in recovery]!=[(33,1),(34,2),(35,1),(36,2),(37,3)]:
            raise ValueError('supervisor recovery attempts')
        if not controllers[0][0]<recovery[0][0]<completed[0][0]<controllers[1][0]<recovery[1][0]<completed[1][0]:
            raise ValueError('automatic recovery lifecycle')
        if recovery[1][1]['dispatches']-recovery[0][1]['dispatches']<5 or sum(completed[0][0]<i<controllers[1][0] for i,e in events)<2:
            raise ValueError('bounded retry delay with application progress')
        calls=rec(b'STYLEOS NATIVE SHELL-CALL',('pid','gen','root','op','reply'))
        submitted=rec(b'STYLEOS NATIVE SHELL-SUBMIT',('key',))
        applied=rec(b'STYLEOS NATIVE SHELL-APPLY',('epoch',))
        if [tuple(x.values()) for _,x in controllers]!=[(pid,pid,roots[2],pid-32) for pid in range(33,38)] or [tuple(x.values()) for _,x in completed]!=[(pid,pid-32,2) for pid in range(33,38)]:
            raise ValueError('controller lifecycle evidence')
        want=[(3,3,roots[2],0x105,0),(33,33,roots[2],0x105,0x100000044),
              (34,34,roots[2],0x105,0x200000044),(34,34,roots[2],0x106,2)]
        want.extend((pid,pid,roots[2],0x105,((pid-32)<<32)|0x44) for pid in (35,36,37))
        if [tuple(x.values()) for _,x in calls]!=want or len(submitted)!=2 or any(x['key']!=0x44 for _,x in submitted) or len(applied)!=1 or applied[0][1]['epoch']!=2:
            raise ValueError('controller authorization, retained input, approval')
        syscalls=[(i,e) for i,e in events if e['vector']==128 and e['rax'] in (0x105,0x106)]
        if len(syscalls)!=7 or any(j+1!=i or x['pid']!=e['pid'] for (j,x),(i,e) in zip(calls,syscalls)):
            raise ValueError('controller receipt not adjacent to actual trap')
        gap=rec(b'STYLEOS NATIVE SHELL-GAP-END',('dispatches',))
        if len(gap)!=1 or not gap[0][0]<controllers[0][0]<per[33][0][0]<retire_by_gen[33][0]<completed[0][0]<controllers[1][0]<per[34][0][0]<retire_by_gen[34][0]<completed[1][0]<applied[0][0]<r0[0]:
            raise ValueError('fault then fresh controller launch and redraw ordering')
        marks=[i for i,x in enumerate(lines) if x==b'STYLEOS DESKTOP SHELL RESTART']
        f10=[i for i,e in inputs if e['key']==0x44 and not e['mouse']]
        if len(marks)!=1 or len(f10)!=1 or not submitted[0][0]<f10[0]<gap[0][0]<applied[0][0]<marks[0]<r0[0]:
            raise ValueError('actual input then controller approval then UI restart')
        for pid in (1,2):
            if not any(controllers[0][0]<i<completed[1][0] and e['vector']==0x50 for i,e in per[pid]):
                raise ValueError('application stopped during controller recovery')
        begin=rec(b'STYLEOS NATIVE SHELL-EXHAUST-BEGIN',('dispatches',))
        end=rec(b'STYLEOS NATIVE SHELL-EXHAUST-END',('dispatches','resident'))
        timeouts=rec(b'STYLEOS NATIVE SHELL-TIMEOUT',('pid','gen','slices','dispatches'))
        exhausted=[i for i,x in enumerate(lines) if x==b'STYLEOS SHELL RECOVERY EXHAUSTED']
        if len(begin)!=1 or len(end)!=1 or len(timeouts)!=3 or len(time_stops)!=3 or len(exhausted)!=1:
            raise ValueError('bounded timeout evidence counts')
        lo=begin[0][0];hi=end[0][0]
        if not marks[0]<lo<submitted[1][0]<controllers[2][0]<exhausted[0]<hi<r0[0] or end[0][1]['resident']!=2:
            raise ValueError('exhaustion interval')
        for k,pid in enumerate((35,36,37)):
            ti,t=timeouts[k];si,s=time_stops[k]
            if tuple(t.values())!=(pid,pid,32,recovery[k+2][1]['dispatches']+32) or tuple(s.values())!=(2,pid,pid,roots[2]):
                raise ValueError('exact timeout identity and budget')
            if not controllers[k+2][0]<recovery[k+2][0]<per[pid][0][0]<ti<retire_by_gen[pid][0]<si<completed[k+2][0]<exhausted[0]:
                raise ValueError('timeout retirement ordering')
            if k<2 and not completed[k+2][0]<controllers[k+3][0]:
                raise ValueError('no overlapping controller attempts')
        for pid in (1,2):
            if sum(lo<i<hi and e['vector']==0x50 for i,e in per[pid])<30 or sum(exhausted[0]<i<hi for i,e in per[pid])<5:
                raise ValueError('peer progress through exhaustion')
        if end[0][1]['dispatches']-begin[0][1]['dispatches']!=sum(lo<i<hi for i,e in events):
            raise ValueError('exhaustion dispatch accounting')
        launch=rec(b'STYLEOS NATIVE WINDOW-LAUNCH',('pid','gen','root','session','window'))
        rejected=rec(b'STYLEOS NATIVE WINDOW-LAUNCH-REFUSALS',('session',))
        if len(launch)!=3 or len(rejected)!=1:raise ValueError('windowed launch receipts')
        sid=rejected[0][1]['session']
        if not sid or not rejected[0][0]<admissions[0][0]:raise ValueError('preflight refusal ordering')
        if [tuple(x.values()) for _,x in launch]!=[(1,1,roots[0],sid,b['window']),(2,2,roots[1],sid,b['peer-window']),(fresh_pid,fresh_pid,roots[0],sid,n['window'])]:
            raise ValueError('windowed launch identity')
        if any(not by_pid[x['pid']][0]<i<per[x['pid']][0][0] for i,x in launch):raise ValueError('window ownership after first dispatch')
        frames=rec(b'STYLEOS NATIVE FRAME',('pid','gen','root','window','va','count','status','first'))
        want=[(2,2,roots[1],0,0x12000ff0,2,1,0),(2,2,roots[1],0,0x12000200,2,0,0x234567),(2,2,roots[1],0,0x12000200,2,1,0)]
        if [tuple(x.values()) for _,x in frames]!=want:raise ValueError('frame acceptance or refusal receipts')
        calls=[(i,e) for i,e in events if e['vector']==128 and e['rax']==0x103]
        if len(calls)!=3 or any(j+1!=i or e['pid']!=2 for (j,_),(i,e) in zip(frames,calls)):
            raise ValueError('frame receipt not adjacent to authenticated trap')
    if shell_boundary:verify_shell_boundary(lines,roots,0x100000df if window_input else 0x10000064 if window_tile else 0x10000024 if window_ipc else 0x1000000d,focus=window_focus,frame=window_frame)
    return result
