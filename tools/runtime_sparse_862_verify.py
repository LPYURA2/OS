"""Delayed registry admission; both physical mappings are pre-audited fixtures."""
import re
from runtime_loop_862_verify import verify as prior, loop_site, trace_check
from fault_site import _unpack,_range

def sparse_site(efi,map_file):
    site=loop_site(efi,map_file);s=map_file.read_text()
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    for name,obj in [('sparse_admit862',r'boot_x86_64_user_preemption_848\.o')]+[(f'rt862_bank_{x}',r'kernel_runtime_bank_862\.o') for x in ('open','admit','release')]:
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+'+obj+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S862S map '+name)
        seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S862S section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S862S executable rights')
        _range(efi,ro+rel,1,'S862S entry byte');site[name+'_rva']=rva
    return site

def verify(raw,rc,site,reference,*,epoch=0):
    try:
        if not all(site.get(x+'_rva') for x in ('sparse_admit862','rt862_bank_open','rt862_bank_admit','rt862_bank_release')):raise ValueError('S862S linked policy')
        ok,obs=prior(raw,rc,site,reference,sparse=True,epoch=epoch)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S862S ')]
        def pos(prefix):
            found=[i for i,x in enumerate(lines) if x.startswith(prefix)]
            if len(found)!=1:raise ValueError('S862S phase count')
            return found[0]
        def row(i,tag,keys):
            m=re.fullmatch(b'STYLEOS S862S '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S862S schema')
            return dict(zip(keys,(int(v,16) for v in m.groups())))
        if len(rows)!=4:raise ValueError('S862S record count')
        r=obs['join_roots'];safe=r['safe'];admissions=[];releases=[]
        for i in range(2):
            identity=(i,i+1+2*epoch,r['gen-'+'ab'[i]],r['ab'[i]],safe)
            a=row(rows[i],b'ADMIT',('id','pid','handle','root','safe','dispatches','processes','windows'))
            z=row(rows[2+i],b'RELEASE',('id','pid','handle','root','safe','processes'))
            if tuple(a.values())!=(*identity,i+10*epoch,i+1,i+1) or tuple(z.values())!=(*identity,1-i):raise ValueError('S862S binding/count')
            admissions.append(a);releases.append(z)
        if not rows[0]<pos(b'STYLEOS S861T READY ')<pos(b'STYLEOS S862B ENTER step=0x0000000000000000 '):raise ValueError('S862S A admission order')
        if not pos(b'STYLEOS S848 BACK step=0x0000000000000000 ')<rows[1]<pos(b'STYLEOS S860 READY ')<pos(b'STYLEOS S862B ENTER step=0x0000000000000001 '):raise ValueError('S862S B admission order')
        if not pos(b'STYLEOS S861T SCRUB ')<rows[2]<rows[3]<pos(b'STYLEOS S862B CLEAN '):raise ValueError('S862S release order')
        obs.update(sparse_admissions=admissions,sparse_releases=releases);return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)
