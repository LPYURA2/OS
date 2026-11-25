"""Strict UART and independent CPU trace proof of S861 supervisor switches."""
import re
from runtime_arena_858_verify import verify as prior, POLICY as ARENA
from irq_trace_accept_859 import HEADER
POLICY=b'STYLEOS S861 POLICY cpl0-only=yes shared-desktop=yes inherited-return=no cpl3=no timer=no production-cr3=deny'
def verify(raw,rc,site,reference,*,generation_base=4,clean_fnv=None,cold=False):
    try:
        if generation_base not in ((1,3) if cold else (4,6)):raise ValueError('861 generation profile')
        ok,obs=prior(raw,rc,site['rva'],reference,cold=cold)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[x for x in lines if x.startswith(b'STYLEOS S861 ')]
        if len(rows)!=5 or rows[-1]!=POLICY or any(b' FAIL ' in x for x in lines):raise ValueError('861 records')
        if [lines.index(x) for x in [ARENA,*rows]]!=sorted(lines.index(x) for x in [ARENA,*rows]):raise ValueError('861 phase order')
        def row(line,tag,keys):
            m=re.fullmatch(b'STYLEOS S861 '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m:raise ValueError('861 schema '+str(tag))
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        def extent(tag):
            v=[x for x in lines if x.startswith(b'STYLEOS MEMMAP '+tag+b' ')]
            if len(v)!=1:raise ValueError('861 extent')
            return tuple(int(x,16) for x in v[0].split()[-2:])
        r=row(rows[0],b'ROOTS',('safe','a','b','pa-a','pa-b','gen-a','gen-b'));safe=obs['active']['root'];lo,hi=extent(b'IMAGE');sl,sh=extent(b'STACK');pa=obs['arena']['base']
        if r['safe']!=safe or (r['pa-a'],r['pa-b'],r['gen-a'],r['gen-b'])!=(pa,pa+3*4096,generation_base,generation_base+1):raise ValueError('861 lease binding')
        # Table spans are disjoint from every prior root/descriptor and stack.
        spans=[(safe,safe+32*4096),(obs['root']['source'],obs['root']['source']+32*4096),(sl,sh),(obs['bridge']['idt'],obs['bridge']['idt']+4096),extent(b'HANDOFF'),extent(b'SOURCE')]+[(v,v+4096) for v in obs['descriptors'].values()]
        for name in ('a','b'):
            x=r[name];end=x+32*4096
            if x%4096 or not lo<=x<end<=hi or any(x<v and u<end for u,v in spans):raise ValueError('861 table overlap')
            spans.append((x,end))
        proof=[]
        for i in range(2):
            v=row(rows[i+1],b'SWITCH',('id','child','safe','rip','rsp','cs','flags','frame','own','other'))
            own=(0x86110000+i)^0x86100000;other=(0x86110000+1-i)^(0x86100000 if i else 0)
            if (v['id'],v['child'],v['safe'],v['rip'],v['cs'],v['frame'],v['own'],v['other'])!=(i,r['ab'[i]],safe,lo+site['child_rva'],obs['bp']['cs'],obs['bp']['frame'],own,other):raise ValueError('861 switch')
            if not sl<=v['rsp']<sh or v['flags']&0x200 or not v['flags']&2:raise ValueError('861 frame')
            proof.append(v)
        clean=row(rows[3],b'CLEAN',('safe','used','next','tables-zero','leases-zero','fnv'))
        if tuple(clean.values())!=(safe,0,generation_base+2,1,1,reference['fnv'] if clean_fnv is None else clean_fnv):raise ValueError('861 cleanup')
        obs.update(join_roots=r,join_switches=proof,join_clean=clean);return True,obs
    except (ValueError,KeyError,TypeError,IndexError,StopIteration) as exc:return False,str(exc)
def trace_check(trace,obs,*,initial_bp=True):
    roots=obs['join_roots'];root=roots['safe'];headers=list(HEADER.finditer(trace));records=[];started=False
    for i,m in enumerate(headers):
        block=trace[m.start():headers[i+1].start() if i+1<len(headers) else len(trace)]
        def one(pattern):
            v=re.findall(pattern,block,re.M)
            if len(v)!=1:raise ValueError('861 CPU field '+pattern)
            return int(v[0],16)
        cr3=one(r'\bCR3=([0-9a-fA-F]+)\b')
        if not started and cr3 not in (root,roots['a'],roots['b']):continue
        started=True
        seq,vec,err,soft,cpl,cs,rip,pc,ss,rsp=m.groups()
        records.append((int(seq),tuple(int(v,16) for v in (vec,err,soft,cpl,cs,rip,pc,ss,rsp))+(cr3,one(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b'),one(r'^RIP=([0-9a-fA-F]+)\b'),one(r'\bRSP=([0-9a-fA-F]+)\b'),one(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),one(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),one(r'^TR\s*=([0-9a-fA-F]+)\b'))))
    if len(records)!=(3 if initial_bp else 2) or any(b[0]!=a[0]+1 for a,b in zip(records,records[1:])):raise ValueError('861 CPU sequence')
    active=obs['active']
    points=[obs['bp'],*obs['join_switches']] if initial_bp else obs['join_switches']
    cr3s=[root,roots['a'],roots['b']] if initial_bp else [roots['a'],roots['b']]
    for (_,v),p,cr3 in zip(records,points,cr3s):
        rip=p['rip']-1
        expected=(3,0,1,0,p['cs'],rip,rip,p['cs']-8,p['rsp'],cr3,p['flags'],rip,p['rsp'],active['gdt'],active['idt'],active['tr'])
        if v!=expected:raise ValueError('861 CPU binding')
    return records
