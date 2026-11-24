"""Verify real supervisor-only switches to two distinct guarded roots."""
import re
from physical_arena_844_verify import verify as prior


def verify(raw,rc):
    try:
        ok,obs=prior(raw,rc)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines()
        rec=[x for x in lines if x.startswith(b'STYLEOS S846 ')]
        policy=b'STYLEOS S846 POLICY supervisor-switch=yes cpl3=no production-cr3=deny'
        def parse(line,tag,keys):
            prefix=('STYLEOS S846 '+tag+' ').encode()
            if not line.startswith(prefix):raise ValueError('order '+tag)
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
            if not m:raise ValueError('schema '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        if len(rec)!=5 or rec[-1]!=policy:raise ValueError('record count/policy')
        r=parse(rec[0],'READY',('safe','a','b','pa-a','pa-b','gen-a','gen-b','used-a','used-b','ist1','ist2','ist3'))
        root=obs['root']['new'];base=obs['reserve']['base']
        lo,hi=[int(x,16) for x in next(x for x in lines if x.startswith(b'STYLEOS MEMMAP IMAGE ')).split()[-2:]]
        if (r['safe'],r['pa-a'],r['pa-b'],r['gen-a'],r['gen-b'])!=(root,base,base+12288,4,5):raise ValueError('lease provenance')
        if not all(1<=r[k]<=32 for k in ('used-a','used-b')):raise ValueError('table capacity')
        spans=[(r[k],r[k]+size) for k,size in [('safe',4096),('a',32*4096),('b',32*4096),('ist1',4096),('ist2',4096),('ist3',4096)]]
        if not all(lo<=x<y<=hi and x%4096==0 for x,y in spans):raise ValueError('image extents')
        if any(x<v and u<y for i,(x,y) in enumerate(spans) for u,v in spans[:i]):raise ValueError('table/IST collision')
        switches=[]
        for i,line in enumerate(rec[1:3]):
            v=parse(line,'SWITCH',('id','live','after','code','data','stack','write-data','write-stack','foreign-data','g1','g2','g3'))
            if (v['id'],v['live'],v['after'],v['code'],v['data'],v['stack'],v['write-data'],v['write-stack'],v['foreign-data'],v['g1'],v['g2'],v['g3'])!=(i,r['ab'[i]],root,0x846c0000+i,0x846d0000+i,0x846e0000+i,0x846d1000+i,0x846e1000+i,0x846d1000 if i else 0x846d0001,0x84610000,0x84610001,0x84610002):raise ValueError('switch evidence')
            switches.append(v)
        clean=parse(rec[3],'CLEAN',('root','used','tables-zero','leases-zero'))
        if tuple(clean.values())!=(root,0,1,1):raise ValueError('cleanup')
        prev=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S844 POLICY '))
        end=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 READY '))
        indices=[prev]+[lines.index(x) for x in rec]+[end]
        if any(x>=y for x,y in zip(indices,indices[1:])):raise ValueError('phase order')
        obs.update(user_roots_supervisor_verified=True,child_roots=r,switches=switches,child_cleanup=clean)
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
