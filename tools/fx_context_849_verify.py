"""Separate S849 evidence gate. Old S848 integer-only verification stays default."""
import re
from process_trial_847_verify import verify as prior
from user_preempt_848_verify import verify_stage as integer_stage, verify_link_map as integer_map, ORDER, fixture_order
POLICY=b'STYLEOS S849 POLICY cpl3-timer=yes fx-legacy=yes single-cpu=yes production-cr3=deny'
CLEAN=b'STYLEOS S849 CLEAN slots-zero=yes kernel-copy-zero=yes'

def parse(line,tag,keys):
    prefix=('STYLEOS S849 '+tag+' ').encode()
    if not line.startswith(prefix):raise ValueError('S849 order '+tag)
    m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
    if not m:raise ValueError('S849 schema '+tag)
    return dict(zip(keys,(int(x,16) for x in m.groups())))

def verify_fx(lines,obs,*,after_tag=b'STYLEOS S846 CLEAN ',budgets=(3,5)):
    rec=[x for x in lines if x.startswith(b'STYLEOS S849 ')]
    if len(rec)!=14 or rec[-2:]!=[CLEAN,POLICY]:raise ValueError('S849 records/policy')
    mode=parse(rec[0],'MODE',('ecx','edx','cr0','cr4'))
    need=(1<<0)|(1<<24)|(1<<25)|(1<<26)
    if mode['ecx']>>32 or mode['edx']>>32 or mode['edx']&need!=need or mode['ecx']&(1<<27) or mode['cr0']&0x22!=0x22 or mode['cr0']&0xc or mode['cr4']&0x600!=0x600 or mode['cr4']&(1<<18):raise ValueError('S849 mode')
    ready=parse(rec[1],'READY',('mask','safe'));roots=obs['child_roots']
    if not ready['mask'] or ready['mask']&~0xffff or ready['mask']&0x3f80!=0x3f80 or ready['safe']!=roots['safe']:raise ValueError('S849 admission')
    counts=[0,0];captures=[]
    s848=[(i,x) for i,x in enumerate(lines) if x.startswith(b'STYLEOS S848 ')]
    if len(s848)!=25:raise ValueError('S849 parent record count')
    indices=[lines.index(x) for x in rec]
    if not s848[1][0]<indices[0]<indices[1]<s848[2][0]:raise ValueError('S849 setup ordering')
    for step,(id,terminal) in enumerate(fixture_order(budgets)):
        counts[id]+=1
        v=parse(rec[2+step],'CAPTURE',('id','root','n','xmm14','xmm15','mxcsr','st0','exp','kernel-restored','terminal'))
        tag=0x8480000000000000|(id<<32)
        if tuple(v.values())!=(id,roots['ab'[id]],counts[id],tag|13,tag|14,0x3f80 if id else 0x1f80,0 if id else 0x8000000000000000,0 if id else 0x3fff,1,int(terminal)):raise ValueError('S849 FX snapshot')
        previous=s848[2+2*step][0];event=s848[3+2*step][0];back=s848[4+2*step][0]
        if not previous<indices[2+step]<event<back:raise ValueError('S849 capture ordering')
        captures.append(v)
    after=next(i for i,x in enumerate(lines) if x.startswith(after_tag))
    if counts!=[v+1 for v in budgets] or not s848[22][0]<indices[-2]<s848[23][0]<s848[24][0]<indices[-1]<after:raise ValueError('S849 cleanup ordering')
    return dict(fx_context_verified=True,fx_mode=mode,fx_ready=ready,fx_captures=captures,fx_scrubbed=True)

def verify(raw,rc):
    try:
        ok,obs=prior(raw,rc)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();obs.update(integer_stage(lines,obs,fx=True));obs.update(verify_fx(lines,obs));return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)

def verify_link_map(path):
    integer_map(path,fx=True)
