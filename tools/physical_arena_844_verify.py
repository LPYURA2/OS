"""Validate the bounded external EFI allocation and real S843 IRQ run."""
import re
from timer_scheduler_843_verify import verify as prior
def verify(raw,rc,*,external_stack=None):
    try:
        ok,obs=prior(raw,rc,external_stack=external_stack)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines()
        rec=[x for x in lines if x.startswith(b'STYLEOS S844 ')]
        policy=b'STYLEOS S844 POLICY bounded-efi-arena=yes candidate-grants=no reclaim=no dma-grants=no production-cr3=deny'
        def parse(line,tag,keys):
            prefix=('STYLEOS S844 '+tag+' ').encode()
            if not line.startswith(prefix):raise ValueError('order '+tag)
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line[len(prefix):])
            if not m:raise ValueError('schema '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        if len(rec)!=4 or rec[-1]!=policy:raise ValueError('record count/policy')
        a=parse(rec[0],'RESERVE',('base','pages'))
        b=parse(rec[1],'ADMIT',('base','end','desc-start','desc-end','type','attr'))
        c=parse(rec[2],'LIVE',('root','a','b','reuse','gen-a','gen-b','gen-reuse','used','zero','refusals'))
        start=a['base'];end=start+65536
        if not 0x100000<=start<end<=0x08000000 or start%4096 or a['pages']!=16 or (b['base'],b['end'])!=(start,end) or not b['desc-start']<=start<end<=b['desc-end'] or b['desc-start']%4096 or b['desc-end']%4096 or b['type']!=2 or not b['attr']&8 or b['attr']>>63:raise ValueError('EFI receipt')
        def ranges(tag):
            return [tuple(int(v,16) for v in line.split()[-2:]) for line in lines if line.startswith(('STYLEOS MEMMAP '+tag+' ').encode())]
        if not any(lo<=start<end<=hi for lo,hi in ranges('RESERVED')):raise ValueError('reserve')
        for tag in ('IMAGE','STACK','HANDOFF','SOURCE','RAM','CANDIDATE'):
            if any(lo<end and start<hi for lo,hi in ranges(tag)):raise ValueError('overlap '+tag)
        if (c['root'],c['a'],c['b'],c['reuse'],c['gen-a'],c['gen-b'],c['gen-reuse'],c['used'],c['zero'],c['refusals'])!=(obs['root']['new'],start,start+16384,start,1,2,3,0,1,4):raise ValueError('allocation lifecycle')
        marker=lines.index(b'STYLEOS x86_64 ExitBootServices OK')
        own=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S842 READY '))
        sched=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 READY '))
        if not lines.index(rec[0])<marker<lines.index(rec[1])<own<lines.index(rec[2])<lines.index(policy)<sched:raise ValueError('phase order')
        if external_stack is not None and external_stack!=(start,end):raise ValueError('external stack not in admitted arena')
        obs.update(physical_arena_verified=True,reserve=a,admission=b,lifecycle=c)
        return True,obs
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
