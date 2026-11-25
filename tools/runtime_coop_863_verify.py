"""S863 cooperative bank proof: raw UART plus independently recorded CPU events."""
import re
from runtime_user_861_verify import user_site,trace_check,HEADER
from runtime_roots_861_verify import verify as roots_verify
def verify(raw,rc,site,ref):
    try:
        ok,obs=roots_verify(raw,rc,site,ref)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines()
        def rows(prefix):return [x for x in lines if x.startswith(prefix)]
        def row(line,prefix,keys):
            m=re.fullmatch(prefix+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m:raise ValueError('S863C schema '+str(prefix))
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        r=obs['join_roots'];a=obs['active'];image=int(rows(b'STYLEOS MEMMAP IMAGE ')[0].split()[-2],16)
        u=rows(b'STYLEOS S861U ');p=rows(b'STYLEOS S847 ');c=rows(b'STYLEOS S863C ')
        if len(u)!=3 or len(p)!=2 or len(c)!=16:raise ValueError('S863C count')
        ready=row(u[0],b'STYLEOS S861U READY',('safe','pid-a','pid-b','win-a','win-b','idt'))
        if tuple(ready.values())!=(r['safe'],1,2,1,2,image+site['user_idt_rva']):raise ValueError('S863C ready')
        sel=row(p[0],b'STYLEOS S847 READY',('safe','cs','ss','tr','limit','a-ok','a-deny','a-yield','a-fault','b-ok','b-deny','b-yield','b-exit'))
        pcs=((0x1000001a,0x10000039,0x10000062,0x10000078),(0x1000001a,0x10000039,0x10000062,0x1000007e))
        tr=sel['tr'];cs=(tr+16)|3;ss=(tr+24)|3
        if tuple(sel.values())!=(r['safe'],cs,ss,a['tr'],tr+31,*pcs[0],*pcs[1]):raise ValueError('S863C selectors')
        traps=[];frames=[];commits=[]
        for n in range(8):
            i=n%2;j=n//2;fault=n==6
            t=row(c[2*n],b'STYLEOS S863C TRAP',('n','id','stage','root','vector','error','cr2','rip','rsp','cs','ss','frame','call','flags'))
            if tuple(t.values())[:-1]!=(n,i,j,r['ab'[i]],14 if fault else 128,5 if fault else 0,0x400000003000 if fault else 0,pcs[i][j],0x11000ff8 if j==2 else 0x11001000,cs,ss,0x400000003000+(0x3000 if fault else 0)+4096-16-176,0 if fault else 3 if j<2 else 1 if j==2 else 2) or not t['flags']&2 or t['flags']&~0x8d7:raise ValueError('S863C trap')
            v=row(c[2*n+1],b'STYLEOS S863C COMMIT',('n','id','pid','dispatch','safe','pc','rax','state','windows','value','foreign','stack-same'))
            # PR_READY=1, PR_EXITED=4, PR_FAULTED=5; see process_runtime.h.
            state=1 if n<6 else 5 if fault else 4
            if tuple(v.values())!=(n,i,i+1,n+1,r['safe'],pcs[i][j] if n<6 else 0,3 if j==1 else 0,state,2 if n<6 else 7-n,0x846d1000+i if j<2 else 0x847101 if n==7 else 0x847000+i,0x846d1000+1-i if n<5 else 0x847000+1-i,1):raise ValueError('S863C commit')
            traps.append(t);frames.append({'n':n,'flags':t['flags']});commits.append(v)
        clean=row(u[1],b'STYLEOS S861U CLEAN',('safe','dispatches','processes','windows'))
        if tuple(clean.values())!=(r['safe'],8,0,0) or p[1]!=b'STYLEOS S847 POLICY cpl3=yes user-fault-contained=yes range-only=yes if=0 production-cr3=deny' or u[2]!=b'STYLEOS S861U POLICY cooperative-cpl3=yes existing-dispatcher=yes timer=no fp=no production-cr3=deny':raise ValueError('S863C cleanup')
        order=[u[0],p[0],*c,p[1],u[1],u[2]]
        if [lines.index(x) for x in order]!=sorted(lines.index(x) for x in order):raise ValueError('S863C phase order')
        obs.update(process_traps=traps,user_frames=frames,user_ready=ready,coop_commits=commits,user_clean=clean);return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)
