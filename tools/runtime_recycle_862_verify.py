"""Two actual sparse cycles: never manufacture or rewrite captured evidence."""
import re
from runtime_sparse_862_verify import verify as sparse_verify, sparse_site, trace_check as sparse_trace
from runtime_roots_861_verify import HEADER
from fault_site import _unpack, _range

def recycle_site(efi,map_file):
    site=sparse_site(efi,map_file);s=map_file.read_text()
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    for name,obj,executable in [('s849_guest_bind_leases',r'boot_x86_64_fx_guest_849\.o',True),('stale862',r'boot_x86_64_user_preemption_848\.o',False)]:
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S862R map '+name)
        seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S862R section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<vs or bool(flags&0x20000000)!=executable or bool(flags&0x80000000)==executable:raise ValueError('S862R symbol rights')
        if executable:
            if rel>=rs:raise ValueError('S862R unbacked code')
            _range(efi,ro+rel,1,'S862R entry')
        site[name+'_rva']=rva
    return site

def verify(raw,rc,site,reference,*,clean_fnv=None,cold=False):
    try:
        if not site.get('s849_guest_bind_leases_rva') or not site.get('stale862_rva'):raise ValueError('S862R linked policy')
        lines=raw.splitlines()
        if any(b' FAIL ' in x for x in lines):raise ValueError('S862R guest failure')
        def indices(tag):return [i for i,x in enumerate(lines) if x.startswith(tag)]
        rows=indices(b'STYLEOS S862R ');begins=indices(b'STYLEOS S862R BEGIN ');ends=indices(b'STYLEOS S862R DONE ')
        fx=indices(b'STYLEOS S862R FX ');reuse=indices(b'STYLEOS S862R REUSE ');stale=indices(b'STYLEOS S862R STALE ')
        if tuple(map(len,(rows,begins,ends,fx,reuse,stale)))!=(8,2,2,2,1,1):raise ValueError('S862R record count')
        if not begins[0]<fx[0]<ends[0]<begins[1]<reuse[0]<fx[1]<stale[0]<ends[1] or ends[0]+1!=begins[1] or ends[1]!=len(lines)-1:raise ValueError('S862R phase order')
        if any(x.startswith((b'STYLEOS S861 ',b'STYLEOS S861T ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S862B ',b'STYLEOS S862S ',b'STYLEOS S862L ')) for x in lines[:begins[0]]):raise ValueError('S862R event before round')
        def row(i,tag,keys):
            m=re.fullmatch(b'STYLEOS S862R '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S862R schema '+str(tag))
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        def within(round,tag):
            found=[i for i in range(begins[round]+1,ends[round]) if lines[i].startswith(tag)]
            if len(found)!=1:raise ValueError('S862R phase '+str(tag))
            return found[0]
        prefix=lines[:begins[0]];rounds=[]
        for epoch in range(2):
            # Shared, genuine boot prefix plus the unmodified records of this round.
            # Each original line is retained; only the other round is excluded.
            part=prefix+lines[begins[epoch]+1:ends[epoch]]
            ok,obs=sparse_verify(b'\n'.join(part)+b'\n',rc,site,reference,epoch=epoch,clean_fnv=clean_fnv,cold=cold)
            if not ok:raise ValueError('round '+str(epoch)+': '+str(obs))
            root=obs['join_roots'];safe=root['safe'];gen=(1 if cold else 4)+2*epoch
            begin=row(begins[epoch],b'BEGIN',('round','safe','dispatches','next'))
            end=row(ends[epoch],b'DONE',('round','safe','dispatches','next','next-pid','next-window','used'))
            f=row(fx[epoch],b'FX',('safe','gen-a','gen-b','refused'))
            if tuple(begin.values())!=(epoch,safe,10*epoch,gen) or tuple(end.values())!=(epoch,safe,10*(epoch+1),gen+2,3+2*epoch,3+2*epoch,0) or tuple(f.values())!=(safe,gen,gen+1,2*epoch):raise ValueError('S862R counter/FP binding')
            if not within(epoch,b'STYLEOS S849 READY ')<fx[epoch]<within(epoch,b'STYLEOS S848 READY ') or within(epoch,b'STYLEOS S861 POLICY ')+1!=ends[epoch]:raise ValueError('S862R round boundary')
            rounds.append(obs)
        a,b=(o['join_roots'] for o in rounds)
        if any(a[k]!=b[k] for k in ('safe','a','b','pa-a','pa-b')):raise ValueError('S862R no physical/root reuse')
        r=row(reuse[0],b'REUSE',('pa-a','pa-b','old-a','old-b','new-a','new-b','stale-leases','stale-root'))
        if tuple(r.values())!=(a['pa-a'],a['pa-b'],a['gen-a'],a['gen-b'],b['gen-a'],b['gen-b'],2,1) or not reuse[0]<within(1,b'STYLEOS S861 ROOTS '):raise ValueError('S862R lease proof')
        t=row(stale[0],b'STALE',('slot','root','old-pid','new-pid','old-handle','new-handle','old-dispatch','new-dispatch','refused'))
        if tuple(t.values())!=(0,a['a'],1,3,a['gen-a'],b['gen-a'],1,11,3) or not within(1,b'STYLEOS S862L READY ')<stale[0]<within(1,b'STYLEOS S862B ENTER step=0x0000000000000000 '):raise ValueError('S862R stale ticket proof')
        if rounds[0]['join_clean']['fnv']!=rounds[1]['join_clean']['fnv']:raise ValueError('S862R frame continuity')
        return True,dict(rounds=rounds,reused_pages=r,stale_ticket=t)
    except (ValueError,TypeError,KeyError,IndexError,StopIteration) as exc:return False,str(exc)

def trace_check(text,obs):
    h=list(HEADER.finditer(text));rounds=obs['rounds'];r=rounds[0]['join_roots'];roots=(r['safe'],r['a'],r['b']);start=None
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)];cr=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if len(cr)!=1:raise ValueError('S862R CPU CR3')
        if int(cr[0],16) in roots:start=i;break
    if start is None or len(h)-start!=25:raise ValueError('S862R total events')
    split=h[start+13].start()
    first=sparse_trace(text[:split],rounds[0]);second=sparse_trace(text[split:],rounds[1],initial_bp=False)
    if len(first)!=13 or len(second)!=12 or first[-1][0]+1!=second[0][0]:raise ValueError('S862R CPU continuity')
    return first+second
