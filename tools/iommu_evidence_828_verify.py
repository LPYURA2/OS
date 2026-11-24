"""Independent replay of the supported S812 inventory; NOT IOMMU ownership."""
import hashlib
import re
from ecam_quarantine_827_verify import verify as parent
H = r'0x([0-9a-f]{16})'
POLICY = 'STYLEOS S828 POLICY observation-only=yes unit-ownership=no dma-grants=no ecam=not-read production-cr3=deny'
BACKEND = {'disabled':0,'intel':1,'bridge-intel':1,'bridge-two-hop-intel':1,'amd':2}

def need(ok, why):
    if not ok: raise ValueError(why)

def le(b): return int.from_bytes(b,'little')

def topology(rows):
    need(0 < len(rows) <= 4096, 'function count')
    funcs={}; children={}; buses={0:255}
    for rid,kind,sec,sub in rows:
        need(0<=rid<=65535 and rid not in funcs and kind in (1,2), 'RID/kind')
        need((kind==1 and sec==sub==0) or (kind==2 and 0<sec<=sub<=255 and sec!=rid>>8 and sec not in children), 'bridge extent')
        funcs[rid]=(kind,sec,sub)
        if kind==2: children[sec]=rid
    need(list(funcs)==sorted(funcs),'RID ordering')
    todo=list(children)
    while todo:
        progress=False
        for sec in todo[:]:
            rid=children[sec];p=rid>>8
            if p not in buses: continue
            _,_,sub=funcs[rid]
            need(sec not in buses and sub<=buses[p], 'bridge parent extent')
            buses[sec]=sub;todo.remove(sec);progress=True
        need(progress,'cyclic/unreachable bridges')
    need(all(rid>>8 in buses for rid in funcs), 'unreachable function')
    # S812 discovers actual secondary buses, not all buses in subordinate ranges.
    return funcs,buses

def resolve(funcs,buses,bus,path,kind):
    if bus not in buses or not 2<=len(path)<=32 or len(path)%2: return None
    for i in range(0,len(path),2):
        dev,fn=path[i:i+2]
        if dev>31 or fn>7: return None
        rid=(bus<<8)|(dev<<3)|fn
        if rid not in funcs: return None
        k,sec,_=funcs[rid]
        if i+2==len(path): return rid if k==kind else None
        if k!=2: return None
        bus=sec
    return None

def decode(raw,backend,funcs,buses):
    units=[]; exact={}; aliases={}; unresolved=exceptions=segments=0
    include_all=bridge_scopes=0
    def claim(rid,unit,source):
        need(rid not in aliases,'direct/alias collision')
        if rid in exact: need(exact[rid][0]==unit,'cross descriptor exact collision')
        else: exact[rid]=(unit,source)
    def alias(rid,target,unit):
        need(rid not in exact,'alias/direct collision')
        if rid in aliases: need(aliases[rid]==(target,unit),'alias collision')
        else: aliases[rid]=(target,unit)
    if backend==0: need(not raw,'unexpected IOMMU table')
    else:
        need(48<=len(raw)<=4096 and raw[:4]==(b'DMAR' if backend==1 else b'IVRS') and le(raw[4:8])==len(raw) and sum(raw)%256==0,'ACPI table')
        if backend==1: need(32<=raw[36]<=63 and raw[37]&~3==0,'DMAR header')
        pos=48
        while pos<len(raw):
            need(pos+4<=len(raw),'block header');kind=le(raw[pos:pos+2]) if backend==1 else raw[pos];size=le(raw[pos+2:pos+4]);need(4<=size<=len(raw)-pos,'block length')
            v=raw[pos:pos+size];pos+=size
            is_unit=kind==0 if backend==1 else kind in (0x10,0x11,0x40)
            if not is_unit:
                if (backend==1 and kind==1) or (backend==2 and kind in (0x20,0x21,0x22)):
                    need(size>=(24 if backend==1 else 32),'memory exception shape');exceptions+=1
                else: unresolved+=1
                continue
            fixed=16 if backend==1 else 24 if kind==0x10 else 40
            need(size>=fixed and len(units)<16,'unit extent')
            base=le(v[8:16]);seg=le(v[6:8] if backend==1 else v[16:18]);rid=None if backend==1 else le(v[4:6])
            need(base>0 and base%4096==0 and base<=2**64-4096,'unit base')
            need(not any(u['base']==base and (backend==1 or u['type']==kind) for u in units),'duplicate descriptor')
            need(not any(u['base']==base and (u['segment'],u['requester'])!=(seg,rid) for u in units),'conflicting unit identity')
            unit=len(units);units.append(dict(base=base,segment=seg,requester=rid,type=kind))
            if backend==1:
                need(seg==0 and v[4]&~1==0 and (v[4]&1 or size>16),'DRHD flags/segment')
                include_all+=v[4]&1
                s=fixed
                while s<size:
                    need(s+8<=size,'scope header');k,n=v[s:s+2]
                    need(1<=k<=5 and 8<=n<=size-s and not (n-6)%2,'scope size')
                    found=resolve(funcs,buses,v[s+5],v[s+6:s+n],k) if k in (1,2) else None
                    if found is None:unresolved+=1
                    elif k==1:claim(found,unit,1)
                    else:bridge_scopes+=1
                    s+=n
            else:
                if seg:segments+=1;unresolved+=1;continue
                s=fixed;pending=None
                while s<size:
                    k=v[s]
                    if k==0xf0:unresolved+=1;break
                    n=16 if k&0x80 else 8 if k&0x40 else 4
                    need(s+n<=size,'IVHD entry extent');q=v[s:s+n];s+=n;id=le(q[1:3])
                    if pending:
                        typ,start,target=pending;need(k==4 and id>=start,'IVHD range end')
                        if id-start+1>256:unresolved+=1
                        else:
                            for j in range(start,id+1):
                                if typ==0x43:alias(j,target,unit)
                                else:claim(j,unit,3 if typ==0x47 else 2)
                        pending=None
                    elif k==1:include_all+=1
                    elif k in (2,0x46):claim(id,unit,3 if k==0x46 else 2)
                    elif k in (3,0x43,0x47):pending=(k,id,le(q[5:7]))
                    elif k==0x42:alias(id,le(q[5:7]),unit)
                    elif k==4:raise ValueError('orphan IVHD range end')
                    else:unresolved+=1
                need(pending is None,'unterminated IVHD range')
        need(units,'missing IOMMU descriptor')
    need(len(exact)<=256 and len(aliases)<=256,'claim capacity')
    identities={(u['base'],u['segment'],u['requester']) for u in units}
    # Repeated IVHD forms are not independent physical units; no ownership inference.
    return dict(units=units,distinct_unit_identities=len(identities),exact=[(k,*v) for k,v in exact.items()],aliases=[(k,*v) for k,v in aliases.items()],unresolved=unresolved,exceptions=exceptions,segments=segments,include_all=include_all,bridge_scopes=bridge_scopes)

def vals(line,prefix,keys):
    m=re.fullmatch(prefix+''.join(' '+k+'='+H for k in keys),line)
    need(m,'record shape '+prefix);return tuple(int(v,16) for v in m.groups())

def verify(raw,rc,mode):
    ok,d=parent(raw,rc,mode)
    if not ok:return False,{}
    try:
        lines=raw.decode('ascii').replace('\r','').splitlines();rows=[x for x in lines if x.startswith('STYLEOS S828 ')]
        need(len(rows)>=3 and rows[-1]==POLICY,'S828 records')
        m=re.fullmatch('STYLEOS S828 TABLE backend='+H+' data=(none|[0-9a-f]+)',rows[0]);need(m and int(m[1],16)==BACKEND[mode] and len(m[2])<=8192,'table record')
        blob=b'' if m[2]=='none' else bytes.fromhex(m[2]);backend=BACKEND[mode]
        fs=[];ex=[];al=[];phase=0
        for line in rows[1:-1]:
            if line.startswith('STYLEOS S828 FUNCTION ') and phase==0:fs.append(vals(line,'STYLEOS S828 FUNCTION',('rid','kind','secondary','subordinate')))
            elif line.startswith('STYLEOS S828 EXACT ') and phase<=1:phase=1;ex.append(vals(line,'STYLEOS S828 EXACT',('rid','unit','source')))
            elif line.startswith('STYLEOS S828 ALIAS '):phase=2;al.append(vals(line,'STYLEOS S828 ALIAS',('from','to','unit')))
            else:raise ValueError('record ordering')
        start=lines.index(rows[0]);need(lines[start:start+len(rows)]==rows and lines[start-1]=='STYLEOS S824 POLICY observation-only=yes ecam=not-read dma-grants=no production-cr3=deny' and lines[start+len(rows)].startswith('STYLEOS S816 PLATFORM '),'S828 position')
        funcs,buses=topology(fs);need(len(funcs)==d['functions'] and len(buses)==d['buses'] and len(buses)-1==d['bridges'] and sorted(buses)==d['s824_live_buses'],'topology/parent mismatch')
        audit=decode(blob,backend,funcs,buses);need(ex==audit['exact'] and al==audit['aliases'],'claims mismatch')
        scope=next(x for x in lines if x.startswith('STYLEOS S816 SCOPE '));old=vals(scope,'STYLEOS S816 SCOPE',('units','exact','unresolved','aliases','exceptions','segments-unresolved','collisions'))
        need(old==(len(audit['units']),len(ex),audit['unresolved'],len(al),audit['exceptions'],audit['segments'],0),'scope count mismatch')
        direct=sum(r in funcs for r,_,_ in ex);alias_live=sum(r in funcs for r,_,_ in al)
        oldpci=vals(next(x for x in lines if x.startswith('STYLEOS S816 PCI ')),'STYLEOS S816 PCI',('explicit-matches','unresolved','alias-sources-present'))
        need(oldpci==(direct,len(funcs)-direct,alias_live),'live claim mismatch')
        # Alias chains/cycles and same-unit targets are reported, never admitted.
        direct_units={r:u for r,u,_ in ex};alias_sources={r for r,_,_ in al}
        alias_correlation=[dict(source=r,target=t,unit=u,same_unit_direct_target=(direct_units.get(t)==u),target_is_alias=t in alias_sources,both_live=r in funcs and t in funcs) for r,t,u in al]
        audit['exact']=[list(x) for x in ex];audit['aliases']=[list(x) for x in al]
        d.update(s828_table_sha256=hashlib.sha256(blob).hexdigest(),s828_topology=[list(x) for x in fs],s828_inventory=audit,s828_alias_correlation=alias_correlation,s828_same_boot_inventory=True,s828_unit_ownership=False,s828_dma_authorized=False,_s828_raw={'iommu-table.bin':blob})
        return True,d
    except (ValueError,TypeError,IndexError,KeyError,StopIteration):return False,{}
