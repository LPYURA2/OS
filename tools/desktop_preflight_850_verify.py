"""Strict detached S850 guest evidence. Never claims CR3 activation."""
import re
POLICY=b'STYLEOS S850 POLICY bar-restored=yes detached-audited=yes descriptors-staged=yes desktop-root-active=no framebuffer-write=no production-cr3=deny'

def verify(raw,rc):
    try:
        if rc!=33:raise ValueError('real QEMU debug-exit required')
        lines=raw.splitlines()
        def record(prefix,keys):
            rows=[x for x in lines if x.startswith(prefix+b' ')]
            if len(rows)!=1:raise ValueError('record count '+str(prefix))
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),rows[0][len(prefix)+1:])
            if not m:raise ValueError('schema '+str(prefix))
            return dict(zip(keys,(int(x,16) for x in m.groups()))),lines.index(rows[0])
        def region(tag):
            prefix=b'STYLEOS MEMMAP '+tag+b' '
            rows=[x for x in lines if x.startswith(prefix)]
            if len(rows)!=1:raise ValueError('region count')
            lo,hi=map(lambda x:int(x,16),rows[0][len(prefix):].split())
            if lo>=hi:raise ValueError('region span')
            return lo,hi
        g,gi=record(b'STYLEOS S850 GOP',('base','bytes','width','height','stride','format'))
        b,bi=record(b'STYLEOS S850 BAR',('dev','fn','command','raw','mask','size'))
        r,ri=record(b'STYLEOS S850 ROOT',('inherited','live','detached','source','used','pages','pat-index','cache-bits','pat'))
        d,di=record(b'STYLEOS S850 DESCRIPTORS',('gdt','idt','tss','rsp0','ist1','ist2','ist3'))
        a,ai=record(b'STYLEOS S842 READY',('old','new','low-table','low-used','lapic','ioapic','uc-index','pat','ist'))
        rec=[x for x in lines if x.startswith(b'STYLEOS S850 ')]
        ebs=b'STYLEOS x86_64 ExitBootServices OK';mem=b'STYLEOS MEMMAP PASS'
        if lines.count(ebs)!=1 or lines.count(mem)!=1 or len(rec)!=5 or rec[-1]!=POLICY or not lines.index(ebs)<lines.index(mem)<ai<gi<bi<ri<di<lines.index(POLICY):raise ValueError('phase/policy')
        if any(x.startswith(b'STYLEOS S807 PASS') for x in lines):raise ValueError('unexpected active trial')
        size=4*g['stride']*g['height'];span=(size+4095)&~4095
        if not (320<=g['width']<=4096 and 240<=g['height']<=4096 and g['width']<=g['stride']<=4096 and g['format'] in (0,1) and g['base']>=0x100000 and g['base']%4096==0 and span<=g['bytes']<=b['size'] and span<=16777216 and r['pages']==span//4096):raise ValueError('GOP extent')
        mask=b['mask']&0xfffffff0;sz=(-mask)&0xffffffff
        if not (0<=b['dev']<32 and 0<=b['fn']<8 and b['command']<=65535 and b['command']&2 and b['raw']&0xfffffff0==g['base'] and not b['raw']&7 and b['raw']<2**32 and b['mask']<2**32 and b['mask']&15==b['raw']&15 and sz==b['size'] and sz>=4096 and not sz&(sz-1) and not g['base']&(sz-1) and g['base']+sz<=2**32):raise ValueError('BAR')
        idx=r['pat-index'];bits=(8 if idx&1 else 0)|(16 if idx&2 else 0)|(128 if idx&4 else 0)
        if not (r['inherited']==r['live']==a['old'] and r['source']==a['low-table'] and r['pat']==a['pat'] and 1<=r['used']<=32 and 0<=idx<8 and ((r['pat']>>(idx*8))&255)==0 and bits==r['cache-bits'] and a['ist']==0x400000003000):raise ValueError('root/cache')
        lo,hi=region(b'IMAGE');st,en=region(b'STACK');hs,he=region(b'HANDOFF');ss,se=region(b'SOURCE')
        spans=[(r['detached'],r['detached']+32*4096),(r['source'],r['source']+32*4096),(a['new'],a['new']+4*4096)]+[(v,v+4096) for v in d.values()]
        if not all(lo<=x<y<=hi and x%4096==0 for x,y in spans):raise ValueError('image ownership')
        if any(x<v and u<y for i,(x,y) in enumerate(spans) for u,v in spans[:i]+[(st,en),(hs,he),(ss,se)]):raise ValueError('overlap')
        if any(x<=r['inherited']<y for x,y in spans):raise ValueError('inherited overlap')
        for row in lines:
            if row.startswith((b'STYLEOS MEMMAP RAM ',b'STYLEOS MEMMAP CANDIDATE ')):
                x,y=(int(v,16) for v in row.split()[-2:]);
                if x>=y or x<g['base']+span and g['base']<y:raise ValueError('GOP/RAM conflict')
        return True,{'gop':g,'bar':b,'root':r,'descriptors':d}
    except (ValueError,IndexError,KeyError,TypeError) as e:return False,str(e)
