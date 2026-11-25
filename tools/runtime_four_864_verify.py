"""S864 four-context terminal guest; no synthesized UART or CPU evidence."""
import re
from runtime_arena_858_verify import verify as prior
from runtime_mixed_863_verify import row, GPRS, CODE, IST, HEADER
BUDGET=(5,3,4,6)
N=26
SP=0x11001000
def order():
    n=[0]*4;live=[True]*4;cursor=0
    while any(live):
        while not live[cursor]:cursor=(cursor+1)%4
        i=cursor;cursor=(cursor+1)%4
        kind='call' if not n[i] else 'end' if n[i]==BUDGET[i]+1 else 'tick'
        n[i]+=1
        if kind=='end':live[i]=False
        yield i,kind
def verify_batch(lines,site,ref,o,epoch,*,gen0=4,clean_fnv=None):
    try:
        base=4*epoch;offset=N*epoch
        lines=list(lines)
        if any(b' FAIL ' in x for x in lines):raise ValueError('guest failure')
        def rows(tag):return [x for x in lines if x.startswith(tag+b' ')]
        def one(tag,keys):
            rr=rows(tag)
            if len(rr)!=1:raise ValueError('count '+str(tag))
            return row(rr[0],tag,keys)
        def group(tag,keys,count):
            rr=rows(tag)
            if len(rr)!=count:raise ValueError('count '+str(tag))
            return [row(x,tag,keys) for x in rr]
        def eq(x,y,why):
            if tuple(x.values())!=tuple(y):raise ValueError(why)
        def extent(tag):
            r=rows(b'STYLEOS MEMMAP '+tag)
            if len(r)!=1:raise ValueError('extent')
            return tuple(int(x,16) for x in r[0].split()[-2:])
        active=o['active'];safe=active['root'];lo,hi=extent(b'IMAGE');sl,sh=extent(b'STACK')
        roots=group(b'STYLEOS S864 ROOT',('id','safe','child','pa','gen'),4)
        spans=[(safe,safe+32*4096),(o['root']['source'],o['root']['source']+32*4096),(sl,sh),(o['bridge']['idt'],o['bridge']['idt']+4096),extent(b'HANDOFF'),extent(b'SOURCE')]+[(p,p+4096) for p in o['descriptors'].values()]
        for i,r in enumerate(roots):
            child=r['child']
            eq(r,(i,safe,child,o['arena']['base']+i*3*4096,gen0+base+i),'lease')
            end=child+32*4096
            if child%4096 or not lo<=child<end<=hi or any(child<b and a<end for a,b in spans):raise ValueError('table overlap')
            spans.append((child,end))
        switches=group(b'STYLEOS S864 SWITCH',('id','child','safe','rip','rsp','cs','flags','frame'),4)
        for i,p in enumerate(switches):
            eq(p,(i,roots[i]['child'],safe,lo+site['child_rva'],p['rsp'],o['bp']['cs'],p['flags'],o['bp']['frame']),'switch')
            if not sl<=p['rsp']<sh or p['flags']&0x200 or not p['flags']&2:raise ValueError('switch frame')
        ready=one(b'STYLEOS S861T READY',('safe','idt','cs','ss','limit'))
        eq(ready,(safe,lo+site['timer_idt_rva'],0x5b,0x63,0x67),'IDT')
        tasks=group(b'STYLEOS S864 TASK',('id','pid','root','gen','yield','end','term','budget'),4)
        for i,t in enumerate(tasks):
            label='abcd'[i]
            eq(t,(i,i+1+base,roots[i]['child'],gen0+base+i,site[label+'_yield'],site[label+'_end'],site[label+'_term'],BUDGET[i]),'task identity')
        ad=one(b'STYLEOS S848 ADMIT',('lvt','initial','current','div','isr','irr'))
        if not ad['lvt']&0x10000 or ad['lvt']&~0x300ff or ad['div']&~0xb or ad['current']>ad['initial'] or ad['isr'] or ad['irr']:raise ValueError('timer admission')
        eq(one(b'STYLEOS S848 STOP',('lvt','initial','current','isr','irr')),(ad['lvt'],0,0,0,0),'timer stop')
        tags=(b'STYLEOS S862B ENTER',b'STYLEOS S849 CAPTURE',b'STYLEOS S861T FRAME',b'STYLEOS S862B COMMIT',b'STYLEOS S861T BOUNDARY',b'STYLEOS S860 EVENT',b'STYLEOS S864 PROGRESS',b'STYLEOS S848 BACK')
        streams=[rows(t) for t in tags]
        traps=[x for x in lines if x.startswith((b'STYLEOS S863M CALL ',b'STYLEOS S848 TICK ',b'STYLEOS S848 END '))]
        if any(len(s)!=N for s in streams) or len(traps)!=N:raise ValueError('event count')
        returns=group(b'STYLEOS S863M RESUME',('id','pc','rax','ticks'),4)
        ticks=[0]*4;seen=[0]*4;prog=[0]*4;saved=[(CODE,SP,2)]*4;events=[];windows=4
        for step,(i,kind) in enumerate(order()):
            en,fp,frame,commit,boundary,event,progress,back=(s[step] for s in streams)
            t=tasks[i];child=t['root'];tag=0x8480000000000000|(i<<32);end=kind=='end'
            def check(line,prefix,keys,want,why):eq(row(line,prefix,keys),want,why)
            check(en,tags[0],('step','id','pid','dispatch','safe','root','pc','sp','flags'),(step,i,i+1+base,step+1+offset,safe,child,saved[i][0],saved[i][1],saved[i][2]|0x200),'entry')
            seen[i]+=1
            check(fp,tags[1],('id','root','n','xmm14','xmm15','mxcsr','st0','exp','kernel-restored','terminal'),(i,child,seen[i],tag|13,tag|14,0x3f80 if i else 0x1f80,0 if i else 0x8000000000000000,0 if i else 0x3fff,1,int(end)),'FP')
            ff=row(frame,tags[2],('step','flags'));flags=ff['flags']
            if ff['step']!=step or flags&0x202!=0x202 or flags&~0xad7:raise ValueError('flags')
            vec=128 if kind=='call' or (end and i) else 14 if end else 0x50
            err=5 if vec==14 else 0
            if kind=='call':
                rip=t['yield']
                check(traps[step],b'STYLEOS S863M CALL',('id','root','rip','rsp','flags','frame','rax','timer','isr','irr'),(i,child,rip,SP,flags,IST+4096-16-176,1,0,0,0),'yield')
                eq(returns[i],(i,rip,0,0),'yield result')
            elif kind=='tick':
                r=row(traps[step],b'STYLEOS S848 TICK',('n','id','root','rip','rsp','flags','frame','cs','ss','progress','isr','eoi')+GPRS)
                ticks[i]+=1;rip=r['rip']
                if rip not in (t['end']-10,t['end']-7,t['end']-2) or r['progress']<=prog[i]:raise ValueError('IRQ progress')
                eq(r,(sum(ticks),i,child,rip,SP,flags,IST+4096-16-176,0x5b,0x63,r['progress'],0x10000,1,*[0x12000000 if j==5 else tag|j for j in range(15)]),'IRQ')
                prog[i]=r['progress']
            else:
                rip=t['term'];windows-=1
                check(traps[step],b'STYLEOS S848 END',('id','root','rip','vector','error','cr2','state','ticks'),(i,child,rip,vec,err,IST if err else 0,4 if i else 3,sum(ticks)),'terminal')
            state=(4 if i else 5) if end else 1
            saved[i]=(0,0,0) if end else (rip,SP,flags&~0x200)
            check(commit,tags[3],('step','id','pid','state','pc','sp','flags','zero'),(step,i,i+1+base,state,*saved[i],int(end)),'commit')
            check(boundary,tags[4],('step','safe','gdt','idt','pid'),(step,safe,active['gdt'],active['idt'],i+1+base),'safe descriptors')
            check(event,tags[5],('step','id','pid','handle','dispatch','safe','root','ticks','state','resident','window','minimized'),(step,i,i+1+base,gen0+base+i,step+1+offset,safe,child,ticks[i],state,12288,0 if end else i+1+base,0 if end else int(not i)),'registry')
            p=row(progress,tags[6],('step','id','a','b','c','d'))
            values=[p[k] for k in 'abcd']
            if (p['step'],p['id'])!=(step,i) or any(values[j]!=prog[j] for j in range(4) if j!=i) or values[i]<prog[i] or (not end and values[i]!=prog[i]):raise ValueError('cross-process progress')
            prog=values
            check(back,tags[7],('step','id','safe','state','a','b','stack-same'),(step,i,safe,4 if end and i else 3 if end else 1,*prog[:2],1),'safe stack')
            chain=[en,fp,frame,traps[step],commit,boundary,event,progress,back]
            if [lines.index(x) for x in chain]!=sorted(lines.index(x) for x in chain):raise ValueError('event order')
            events.append(dict(id=i,kind=kind,vector=vec,error=err,rip=rip,flags=flags,root=child))
        if ticks!=list(BUDGET) or seen!=[x+2 for x in BUDGET] or windows:raise ValueError('incomplete schedule')
        eq(one(b'STYLEOS S848 TOTAL',('ticks','a','b','config-restored','timer-stopped')),(18,5,3,1,1),'timer cleanup')
        eq(one(b'STYLEOS S862L EMPTY',('dispatches','safe','windows')),(N,safe,0),'empty scheduler')
        eq(one(b'STYLEOS S860 CLEAN',('safe','dispatches','processes','windows')),(safe,N+offset,0,0),'registry cleanup')
        eq(one(b'STYLEOS S862B CLEAN',('safe','bank-zero','pending-zero')),(safe,1,1),'bank cleanup')
        eq(one(b'STYLEOS S864 CLEAN',('safe','used','next','tables-zero','leases-zero','fnv')),(safe,0,gen0+4+base,1,1,ref['fnv'] if clean_fnv is None else clean_fnv),'memory/frame cleanup')
        if rows(b'STYLEOS S849 CLEAN')!=[b'STYLEOS S849 CLEAN slots-zero=yes kernel-copy-zero=yes']:raise ValueError('FP cleanup')
        reuse=group(b'STYLEOS S864 REUSE',('id','pa','old','new'),4 if epoch else 0)
        stale=group(b'STYLEOS S864 STALE',('id','pid','handle','dispatch','refused'),4 if epoch else 0)
        fx=group(b'STYLEOS S864 FX-STALE',('id','gen'),4 if epoch else 0)
        for i in range(4 if epoch else 0):
            eq(reuse[i],(i,roots[i]['pa'],gen0+base+i-4,gen0+base+i),'stale lease')
            eq(stale[i],(i,base-4+i+1,gen0+base+i-4,offset-N+i+1,3),'stale selection')
            eq(fx[i],(i,gen0+base+i-4),'stale FX generation')
        releases=group(b'STYLEOS S862S RELEASE',('id','pid','handle','root','safe','processes'),4)
        for i,x in enumerate(releases):eq(x,(i,base+i+1,gen0+base+i,roots[i]['child'],safe,3-i),'release')
        return True,dict(four_roots=roots,four_switches=switches,four_events=events,timer_ready=ready)
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
def verify(raw,rc,site,ref,*,cold=False,clean_fnv=None):
    try:
        lines=raw.splitlines()
        prefix=raw if not cold else b'\n'.join(lines[:next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863B COLD '))])+b'\n'
        gen0=1 if cold else 4
        ok,o=prior(prefix,rc,site['rva'],ref,cold=cold)
        if not ok:raise ValueError(o)
        lines=raw.splitlines()
        begins=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S864 BATCH-BEGIN ')]
        ends=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S864 BATCH-END ')]
        if len(begins)!=4 or len(ends)!=4 or any(b' FAIL ' in x for x in lines):raise ValueError('batch count')
        batches=[]
        for i,(a,z) in enumerate(zip(begins,ends)):
            if not a<z or (i and a<=ends[i-1]):raise ValueError('batch order')
            for k,tag,dispatch,gen in ((a,b'STYLEOS S864 BATCH-BEGIN',N*i,gen0+4*i),(z,b'STYLEOS S864 BATCH-END',N*(i+1),gen0+4*(i+1))):
                if tuple(row(lines[k],tag,('round','dispatches','next')).values())!=(i,dispatch,gen):raise ValueError('batch identity')
            ok,b=verify_batch(lines[:begins[0]]+lines[a+1:z],site,ref,o,i,gen0=gen0,clean_fnv=clean_fnv)
            if not ok:raise ValueError(b)
            if i and [x['child'] for x in b['four_roots']]!=[x['child'] for x in batches[0]['four_roots']]:raise ValueError('root reuse')
            batches.append(b)
        o['batches']=batches;return True,o
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
def trace_check(text,o):
    h=list(HEADER.finditer(text));active=o['active'];safe=active['root'];roots=[r['child'] for r in o['batches'][0]['four_roots']]
    plan=[('bp',o['bp'],safe,None)]
    for b in o['batches']:
        plan.extend(('bp',p,roots[i],None) for i,p in enumerate(b['four_switches']))
        plan.extend(('user',e,e['root'],b['timer_ready']['idt']) for e in b['four_events'])
    records=[]
    for j,m in enumerate(h):
        block=text[m.start():h[j+1].start() if j+1<len(h) else len(text)]
        def val(p):
            a=re.findall(p,block,re.M)
            if len(a)!=1:raise ValueError('CPU field '+p)
            return int(a[0],16)
        cr3=val(r'\bCR3=([0-9a-fA-F]+)')
        if not records and cr3 not in [safe,*roots]:continue
        k=len(records)
        rec=tuple(int(x,16) for x in m.groups()[1:])+(cr3,val(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)'),val(r'^RIP=([0-9a-fA-F]+)'),val(r'\bRSP=([0-9a-fA-F]+)'),val(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),val(r'^TR\s*=([0-9a-fA-F]+)\b'))
        if k>=len(plan):raise ValueError('extra CPU event')
        kind,p,root,idt=plan[k]
        if kind=='bp':
            rip=p['rip']-1
            want=(3,0,1,0,p['cs'],rip,rip,p['cs']-8,p['rsp'],root,p['flags'],rip,p['rsp'],active['gdt'],active['idt'],active['tr'])
        else:
            e=p;soft=int(e['vector']==128);rip=e['rip']-2*soft
            want=(e['vector'],e['error'],soft,3,0x5b,rip,rip,0x63,SP,root,e['flags'],rip,SP,active['gdt'],idt,active['tr'])
            if e['error'] and val(r'^CR0=[^\n]*\bCR2=([0-9a-fA-F]+)')!=IST:raise ValueError('CR2')
            for t,name in enumerate(GPRS):
                expected=0x12000000 if t==5 else 0x8480000000000000|(e['id']<<32)|t
                if t==0 and e['kind']!='tick':expected=1 if e['kind']=='call' else 2 if e['id'] else IST
                if val(r'\b'+name.upper()+r'\s*=([0-9a-fA-F]+)\b')!=expected:raise ValueError('CPU GPR '+name)
        if rec!=want or (records and int(m[1])!=records[-1][0]+1):raise ValueError('CPU binding')
        records.append((int(m[1]),rec))
    if len(records)!=len(plan) or len(plan)!=121:raise ValueError('CPU count')
    return records
