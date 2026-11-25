"""Strict CPL3 transcript; parent S846/S844/S843 evidence is mandatory."""
import re
from user_roots_846_verify import verify as prior


def verify(raw,rc,*,joined=None):
    try:
        if joined is None:
            ok,obs=prior(raw,rc)
            if not ok:raise ValueError(obs)
        else:
            obs=dict(joined);obs['child_roots']=obs['join_roots']
        lines=raw.splitlines()
        rec=[x for x in lines if x.startswith(b'STYLEOS S847 ')]
        policy=b'STYLEOS S847 POLICY cpl3=yes user-fault-contained=yes range-only=yes if=0 production-cr3=deny'
        if len(rec)!=14 or rec[-1]!=policy:raise ValueError('S847 count/policy')
        def parse(line,tag,keys):
            prefix=('STYLEOS S847 '+tag+' ').encode()
            if not line.startswith(prefix):raise ValueError('S847 order '+tag)
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
            if not m:raise ValueError('S847 schema '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        r=parse(rec[0],'READY',('safe','cs','ss','tr','limit','a-ok','a-deny','a-yield','a-fault','b-ok','b-deny','b-yield','b-exit'))
        parent=obs['child_roots'];root=parent['safe'];tr=r['tr'];cs=r['cs'];ss=r['ss']
        if r['safe']!=root or not 0<tr<4080 or tr%8 or cs!=((tr+16)|3) or ss!=((tr+24)|3) or r['limit']!=tr+31:raise ValueError('S847 selectors')
        # Fixed, relocation-free fixture offsets independently checked against the link map.
        pc=[[0x10000000+x for x in (0x1a,0x39,0x62,0x78)], [0x10000000+x for x in (0x1a,0x39,0x62,0x7e)]]
        if list(r.values())[5:]!=pc[0]+pc[1]:raise ValueError('S847 fixture sites')
        i=1;n=0;traps=[];backs=[]
        for step in range(4):
            id=step%2
            for stage in (range(3) if step<2 else (3,)):
                t=parse(rec[i],'TRAP',('n','id','stage','root','vector','error','cr2','rip','rsp','cs','ss','frame','call','action','result'));i+=1
                fault=step==2;vec=14 if fault else 128;err=5 if fault else 0
                exp=(n,id,stage,parent['ab'[id]],vec,err,0x400000003000 if fault else 0,pc[id][stage],0x11000ff8 if stage==2 else 0x11001000,cs,ss,0x400000003000+(0x3000 if fault else 0)+4096-16-176,0 if fault else 3 if stage<2 else 1 if stage==2 else 2,1 if stage<2 else 2 if stage==2 else 4 if fault else 3,3 if stage==1 else 0)
                if tuple(t.values())!=exp:raise ValueError('S847 trap evidence')
                n+=1;traps.append(t)
            b=parse(rec[i],'BACK',('step','id','root','state','value','foreign','stack-same'));i+=1
            if tuple(b.values())!=(step,id,root,1 if step<2 else 3 if step==2 else 4,0x847101 if step==3 else 0x847000+id,0x846d1001 if step==0 else 0x847000+1-id,1):raise ValueError('S847 back evidence')
            backs.append(b)
        before=next(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S846 SWITCH id=0x0000000000000001 ' if joined is None else b'STYLEOS S861 SWITCH id=0x0000000000000001 '))
        after=next(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S846 CLEAN ' if joined is None else b'STYLEOS S861 CLEAN '))
        indices=[before]+[lines.index(x) for x in rec]+[after]
        if n!=8 or i!=13 or any(x>=y for x,y in zip(indices,indices[1:])):raise ValueError('S847 phase order')
        obs.update(cpl3_verified=True,process_traps=traps,process_returns=backs,process_selectors=r)
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)


def verify_link_map(path):
    """Check expected instruction offsets from the actual linked fixture symbols."""
    symbols={}
    for line in path.read_text().splitlines():
        m=re.search(r'\bs847_([01])_(start|ok|deny|yield|fault|exit)\s+([0-9a-fA-F]{16})\b',line)
        if m:symbols[m[1]+'-'+m[2]]=int(m[3],16)
    for id,last,offset in [('0','fault',0x78),('1','exit',0x7e)]:
        for site,want in [('ok',0x1a),('deny',0x39),('yield',0x62),(last,offset)]:
            if symbols[id+'-'+site]-symbols[id+'-start']!=want:raise ValueError('S847 link map '+id+'-'+site)
