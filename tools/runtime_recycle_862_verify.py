"""Two actual sparse cycles: never manufacture or rewrite captured evidence."""
import re
from runtime_sparse_862_verify import verify as sparse_verify, sparse_site, trace_check as sparse_trace
from runtime_roots_861_verify import HEADER
from fault_site import _unpack, _range

def recycle_site(efi,map_file,*,mixed=False):
    site=sparse_site(efi,map_file,mixed=mixed);s=map_file.read_text()
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

def verify(raw,rc,site,reference,*,clean_fnv=None,cold=False,round_count=2,mixed=False):
    try:
        n=12 if mixed else 10
        if mixed and not cold:raise ValueError('mixed recycle requires cold profile')
        if round_count<2 or (not cold and round_count!=2):raise ValueError('S862R round profile')
        if not site.get('s849_guest_bind_leases_rva') or not site.get('stale862_rva'):raise ValueError('S862R linked policy')
        lines=raw.splitlines()
        if any(b' FAIL ' in x for x in lines):raise ValueError('S862R guest failure')
        def indices(tag):return [i for i,x in enumerate(lines) if x.startswith(tag)]
        rows=indices(b'STYLEOS S862R ');begins=indices(b'STYLEOS S862R BEGIN ');ends=indices(b'STYLEOS S862R DONE ')
        fx=indices(b'STYLEOS S862R FX ');reuse=indices(b'STYLEOS S862R REUSE ');stale=indices(b'STYLEOS S862R STALE ')
        if tuple(map(len,(rows,begins,ends,fx,reuse,stale)))!=(5*round_count-2,round_count,round_count,round_count,round_count-1,round_count-1):raise ValueError('S862R record count')
        if not begins[0]<fx[0]<ends[0] or ends[-1]!=len(lines)-1:raise ValueError('S862R phase order')
        for i in range(1,round_count):
            if not ends[i-1]+1==begins[i]<reuse[i-1]<fx[i]<stale[i-1]<ends[i]:raise ValueError('S862R phase order')
        if any(x.startswith((b'STYLEOS S861 ',b'STYLEOS S861T ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S862B ',b'STYLEOS S862S ',b'STYLEOS S862L ',b'STYLEOS S863M ')) for x in lines[:begins[0]]):raise ValueError('S862R event before round')
        def row(i,tag,keys):
            m=re.fullmatch(b'STYLEOS S862R '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S862R schema '+str(tag))
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        def within(round,tag):
            found=[i for i in range(begins[round]+1,ends[round]) if lines[i].startswith(tag)]
            if len(found)!=1:raise ValueError('S862R phase '+str(tag))
            return found[0]
        prefix=lines[:begins[0]];rounds=[]
        for epoch in range(round_count):
            # Shared, genuine boot prefix plus the unmodified records of this round.
            # Each original line is retained; only the other round is excluded.
            part=prefix+lines[begins[epoch]+1:ends[epoch]]
            ok,obs=sparse_verify(b'\n'.join(part)+b'\n',rc,site,reference,epoch=epoch,clean_fnv=clean_fnv,cold=cold,mixed=mixed)
            if not ok:raise ValueError('round '+str(epoch)+': '+str(obs))
            root=obs['join_roots'];safe=root['safe'];gen=(1 if cold else 4)+2*epoch
            begin=row(begins[epoch],b'BEGIN',('round','safe','dispatches','next'))
            end=row(ends[epoch],b'DONE',('round','safe','dispatches','next','next-pid','next-window','used'))
            f=row(fx[epoch],b'FX',('safe','gen-a','gen-b','refused'))
            if tuple(begin.values())!=(epoch,safe,n*epoch,gen) or tuple(end.values())!=(epoch,safe,n*(epoch+1),gen+2,3+2*epoch,3+2*epoch,0) or tuple(f.values())!=(safe,gen,gen+1,2*int(epoch>0)):raise ValueError('S862R counter/FP binding')
            if not within(epoch,b'STYLEOS S849 READY ')<fx[epoch]<within(epoch,b'STYLEOS S848 READY ') or within(epoch,b'STYLEOS S861 POLICY ')+1!=ends[epoch]:raise ValueError('S862R round boundary')
            rounds.append(obs)
        receipts=[];tickets=[]
        for i in range(1,round_count):
            a,b=(rounds[j]['join_roots'] for j in (i-1,i))
            if any(a[k]!=b[k] for k in ('safe','a','b','pa-a','pa-b')):raise ValueError('S862R no physical/root reuse')
            r=row(reuse[i-1],b'REUSE',('pa-a','pa-b','old-a','old-b','new-a','new-b','stale-leases','stale-root'))
            if tuple(r.values())!=(a['pa-a'],a['pa-b'],a['gen-a'],a['gen-b'],b['gen-a'],b['gen-b'],2,1) or not reuse[i-1]<within(i,b'STYLEOS S861 ROOTS '):raise ValueError('S862R lease proof')
            t=row(stale[i-1],b'STALE',('slot','root','old-pid','new-pid','old-handle','new-handle','old-dispatch','new-dispatch','refused'))
            if tuple(t.values())!=(0,a['a'],2*i-1,2*i+1,a['gen-a'],b['gen-a'],n*(i-1)+1,n*i+1,3) or not within(i,b'STYLEOS S862L READY ')<stale[i-1]<within(i,b'STYLEOS S862B ENTER step=0x0000000000000000 '):raise ValueError('S862R stale ticket proof')
            if rounds[i-1]['join_clean']['fnv']!=rounds[i]['join_clean']['fnv']:raise ValueError('S862R frame continuity')
            receipts.append(r);tickets.append(t)
        if round_count==2:return True,dict(rounds=rounds,reused_pages=receipts[0],stale_ticket=tickets[0],**({'mixed':True} if mixed else {}))
        return True,dict(rounds=rounds,reused_pages=receipts,stale_ticket=tickets,**({'mixed':True} if mixed else {}))
    except (ValueError,TypeError,KeyError,IndexError,StopIteration) as exc:return False,str(exc)

def trace_check(text,obs):
    n=14 if obs.get('mixed') else 12
    if obs.get('mixed'):
        from runtime_mixed_863_verify import trace_check as check
    else:check=sparse_trace
    h=list(HEADER.finditer(text));rounds=obs['rounds'];r=rounds[0]['join_roots'];roots=(r['safe'],r['a'],r['b']);start=None
    for i,m in enumerate(h):
        block=text[m.start():h[i+1].start() if i+1<len(h) else len(text)];cr=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if len(cr)!=1:raise ValueError('S862R CPU CR3')
        if int(cr[0],16) in roots:start=i;break
    if start is None or len(h)-start!=1+n*len(rounds):raise ValueError('S862R total events')
    records=[]
    for i,round in enumerate(rounds):
        lo=0 if not i else h[start+1+n*i].start()
        hi=h[start+1+n*(i+1)].start() if i+1<len(rounds) else len(text)
        part=check(text[lo:hi],round,initial_bp=not i)
        if len(part)!=(n+1 if not i else n) or (i and records[-1][0]+1!=part[0][0]):raise ValueError('S862R CPU continuity')
        records.extend(part)
    return records
