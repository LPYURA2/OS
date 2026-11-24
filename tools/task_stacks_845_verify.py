"""Require real S844 reservation, S843 hardware IRQs and separate stack writes."""
import re
from physical_arena_844_verify import verify as prior
def verify(raw,rc):
    try:
        lines=raw.splitlines()
        rec=[x for x in lines if x.startswith(b'STYLEOS S845 ')]
        def parse(line,tag,keys):
            prefix=('STYLEOS S845 '+tag+' ').encode()
            if not line.startswith(prefix):raise ValueError('order '+tag)
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
            if not m:raise ValueError('schema '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        policy=b'STYLEOS S845 POLICY terminal=yes contexts-cleared=yes cpl3=no production-cr3=deny'
        if len(rec)!=19 or rec[-1]!=policy:raise ValueError('record count/policy')
        a=parse(rec[0],'LEASE',('base','end','a','b','gen-a','gen-b'))
        base=a['base'];end=a['end']
        if (end-base,a['a'],a['b'],a['gen-a'],a['gen-b'])!=(65536,base,base+16384,4,5):raise ValueError('lease extent/generation')
        ok,obs=prior(raw,rc,external_stack=(base,end))
        if not ok:raise ValueError(obs)
        s=obs['scheduler_ready'];root=s['root'];ist=s['ist']
        if (s['a-stack'],s['b-stack'])!=(a['a'],a['b']):raise ValueError('stack identity')
        ready=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 READY '))
        prev=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S844 POLICY '))
        if not prev<lines.index(rec[0])<ready:raise ValueError('lease order')
        prev=ready
        count=[0,0];progress=[0,0];ticks=[]
        for i,line in enumerate(rec[1:17]):
            v=parse(line,'STACK',('n','from','rsp','a','b'));t=obs['ticks'][i];cur=i%2
            if (v['n'],v['from'],v['rsp'])!=(i+1,cur,t['rsp']):raise ValueError('stack frame')
            now=[v['a'],v['b']];p=[t['a'],t['b']]
            if now[cur]<=count[cur] or now[1-cur]!=count[1-cur] or any(not 0<=now[j]-p[j]<=1 for j in range(2)):raise ValueError('stack write trace')
            parent=next(k for k,x in enumerate(lines) if x.startswith(f'STYLEOS S843 TICK n=0x{i+1:016x} '.encode()))
            if not prev<lines.index(line)<parent:raise ValueError('stack tick order')
            prev=parent;count=now;progress=p;ticks.append(v)
        z=parse(rec[17],'RELEASE',('root','rsp','frame','flags','a','b','used','zero','reuse-gen'))
        if (z['root'],z['frame'],z['a'],z['b'],z['used'],z['zero'],z['reuse-gen'])!=(root,obs['ticks'][-1]['frame'],*count,0,1,6) or not ist+256<=z['rsp']<z['frame'] or z['flags']&0x200 or not z['flags']&2:raise ValueError('terminal release')
        if not prev<lines.index(rec[17])<lines.index(policy)<next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 TOTAL ')):raise ValueError('release order')
        obs.update(leased_stacks_verified=True,leases=a,stack_ticks=ticks,release=z)
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
