"""S858 bounded EFI arena plus actual existing runtime entry and frame."""
import re
from runtime_entry_858_verify import verify as prior, POLICY as RUNTIME
POLICY=b'STYLEOS S858 ARENA-POLICY bounded-efi=yes runtime-services=yes frame=yes reclaim=no irq=no dma-grant=no production-cr3=deny'
ARENA_POLICY=b'STYLEOS S844 POLICY bounded-efi-arena=yes candidate-grants=no reclaim=no dma-grants=no production-cr3=deny'

def verify(raw,rc,rva,reference):
    try:
        lines=raw.splitlines()
        final=[x for x in lines if x.startswith(b'STYLEOS S858 ARENA')]
        if len(final)!=2 or final[-1]!=POLICY:raise ValueError('arena final records')
        ok,obs=prior(b'\n'.join(x for x in lines if not x.startswith(b'STYLEOS S858 ARENA'))+b'\n',rc,rva,reference)
        if not ok:raise ValueError(obs)
        rec=[x for x in lines if x.startswith(b'STYLEOS S844 ')]
        if len(rec)!=4 or rec[-1]!=ARENA_POLICY:raise ValueError('S844 records')
        def parse(row,tag,keys):
            m=re.fullmatch(tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),row)
            if not m:raise ValueError('schema '+str(tag))
            return tuple(int(v,16) for v in m.groups())
        a,pages=parse(rec[0],b'STYLEOS S844 RESERVE',('base','pages'))
        base,end,lo,hi,typ,attr=parse(rec[1],b'STYLEOS S844 ADMIT',('base','end','desc-start','desc-end','type','attr'))
        if not (pages==16 and base==a and end==base+65536 and 0x100000<=lo<=base<end<=hi<=2**64-1 and end<=0x08000000 and not ((base|lo|hi)&4095) and typ==2 and attr&8 and not attr>>63):raise ValueError('EFI receipt')
        def ranges(tag):
            out=[]
            for line in lines:
                if line.startswith(('STYLEOS MEMMAP '+tag+' ').encode()):
                    l,h=(int(v,16) for v in line.split()[-2:]);
                    if not 0<=l<h<=2**64:raise ValueError('invalid memory range')
                    out.append((l,h))
            return out
        if not any(l<=base<end<=h for l,h in ranges('RESERVED')):raise ValueError('not reserved')
        for tag in ('IMAGE','STACK','HANDOFF','SOURCE','RAM','CANDIDATE'):
            spans=ranges(tag)
            if not spans or any(l<end and base<h for l,h in spans):raise ValueError('overlap/missing '+tag)
        root=obs['active']['root']
        if parse(rec[2],b'STYLEOS S844 LIVE',('root','a','b','reuse','gen-a','gen-b','gen-reuse','used','zero','refusals'))!=(root,base,base+16384,base,1,2,3,0,1,4):raise ValueError('lease lifecycle')
        if parse(final[0],b'STYLEOS S858 ARENA',('root','base','pages','used','next','zero'))!=(root,base,16,0,4,1):raise ValueError('final arena')
        def index(tag):return next(i for i,x in enumerate(lines) if x.startswith(tag))
        if not (lines.index(rec[0])<lines.index(b'STYLEOS x86_64 ExitBootServices OK')<lines.index(rec[1])<index(b'STYLEOS S842 READY ')<index(b'STYLEOS S851 POLICY ')<lines.index(rec[2])<lines.index(ARENA_POLICY)<index(b'STYLEOS S858 RUNTIME ')<lines.index(RUNTIME)<lines.index(final[0])<lines.index(POLICY)):raise ValueError('phase order')
        obs['arena']={'base':base,'end':end,'pages':16,'used':0,'next':4,'scrubbed':True,'root':root,'production_grant':False};return True,obs
    except (ValueError,TypeError,KeyError,IndexError,StopIteration) as exc:return False,str(exc)
