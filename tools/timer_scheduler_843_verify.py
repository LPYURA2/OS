"""Verify real S843 timer dispatches, preserving S842 and S807 gates."""
import re
from apic_root_842_verify import verify as prior
def verify(raw,rc,*,external_stack=None):
    try:
        ok,obs=prior(raw,rc)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines()
        records=[x for x in lines if x.startswith(b'STYLEOS S843 ')]
        policy=b'STYLEOS S843 POLICY integer-only=yes cpl3=no production-cr3=deny pit-restored=no'
        def parse(line,tag,keys):
            prefix=('STYLEOS S843 '+tag+' ').encode()
            if not line.startswith(prefix):raise ValueError('order '+tag)
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
            if not m:raise ValueError('fields '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        if len(records)!=19 or records[-1]!=policy:raise ValueError('record count/policy')
        a=parse(records[0],'READY',('root','ist','a-code','a-end','a-stack','b-code','b-end','b-stack'))
        if a['root']!=obs['root']['new'] or a['ist']!=obs['root']['ist']:raise ValueError('root')
        lo,hi=[int(x,16) for x in next(x for x in lines if x.startswith(b'STYLEOS MEMMAP IMAGE ')).split()[-2:]]
        ranges=[]
        stack_lo,stack_hi=(lo,hi) if external_stack is None else external_stack
        for t in 'ab':
            if not lo<=a[t+'-code']<a[t+'-end']<=hi or not stack_lo<=a[t+'-stack']<a[t+'-stack']+16384<=stack_hi or a[t+'-stack']%4096:raise ValueError('ownership extent')
            ranges.extend([(a[t+'-code'],a[t+'-end']),(a[t+'-stack'],a[t+'-stack']+16384)])
        if any(x<z2 and z<x2 for i,(x,x2) in enumerate(ranges) for z,z2 in ranges[:i]):raise ValueError('overlap')
        names=('r15','r14','r13','r12','r11','r10','r9','r8','rdi','rsi','rbp','rbx','rdx','rcx','rax')
        progress=[0,0];ticks=[]
        for i,line in enumerate(records[1:17]):
            v=parse(line,'TICK',('n','from','to','root','frame','rip','rsp','a','b','gprs','eoi')+names)
            cur=i%2;t='ab'[cur]
            if (v['n'],v['from'],v['to'],v['root'],v['frame'],v['gprs'],v['eoi'])!=(i+1,cur,1-cur,a['root'],a['ist']+4096-16-160,0x7fff,1):raise ValueError('switch context')
            if not a[t+'-code']<=v['rip']<a[t+'-end'] or v['rsp']!=a[t+'-stack']+16384-16:raise ValueError('task rip/rsp')
            if any(v[name]!=(0x8430000000000000 | (cur<<32) | j) for j,name in enumerate(names)):raise ValueError('raw GPR pattern')
            now=[v['a'],v['b']]
            if now[cur]<=progress[cur] or now[1-cur]!=progress[1-cur]:raise ValueError('independent progress')
            progress=now;ticks.append(v)
        total=parse(records[17],'TOTAL',('ticks','dispatches','a','b'))
        if [total['ticks'],total['dispatches'],total['a'],total['b']]!=[16,17,*progress] or obs['delivery']['hits']!=16:raise ValueError('total')
        obs.update(timer_scheduler_verified=True,scheduler_ready=a,ticks=ticks,total=total)
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
