"""Replay native S827 detached diagnostic range list from S826 exact bytes."""
import re
from acpi_ecam_826_verify import verify as parent
from uefi_map_826 import parse
H=r'0x([0-9a-f]{16})'
HEAD=re.compile(r'STYLEOS S827 ECAM start='+H+r' end='+H+r' explicit-pages='+H+r' gap-pages='+H)
POLICY='STYLEOS S827 POLICY detached-map=yes ram-grants=no ecam=not-read dma-grants=no production-cr3=deny'
PAGE=4096

def need(ok,reason):
    if not ok: raise ValueError(reason)

def merge(ranges):
    out=[]
    for a,b in sorted(ranges):
        if out and a<=out[-1][1]:out[-1][1]=max(out[-1][1],b)
        else:out.append([a,b])
    return out

def expected(d):
    m=d['s826_map_audit'];a,b=m['ecam'];need(not m['conflicts'],'conflicting ECAM')
    explicit=sum((v['intersection'][1]-v['intersection'][0])//PAGE for v in m['covered'])
    gap=(b-a)//PAGE-explicit
    rows=parse(d['_s826_raw']['final-map.bin'],m['descriptor_size'],m['descriptor_version'])
    need(len(rows)<=256,'native map capacity')
    ram=[];reserved=[]
    for r in rows:
        p=[r['start'],r['end_exclusive']];attr=r['attributes']
        (ram if r['type']==7 and attr&8 and not attr&~0x401f else reserved).append(p)
    need(len(reserved)<256,'native reserve capacity');reserved=merge(reserved+[[a,b]])
    candidate=[]
    for lo,hi in ram:
        for x,y in reserved:
            if y<=lo:continue
            if x>=hi:break
            if lo<x:candidate.append([lo,x])
            lo=max(lo,y)
            if lo>=hi:break
        if lo<hi:candidate.append([lo,hi])
    candidate=merge(candidate)
    need(candidate and len(candidate)<=256 and all(y<=a or x>=b for x,y in candidate),'candidate exclusion')
    return dict(ecam=[a,b],explicit_pages=explicit,gap_pages=gap,reserved=reserved,candidates=candidate)

def records(e):
    hx=lambda x:f'0x{x:016x}'
    a,b=e['ecam']
    out=[f"STYLEOS S827 ECAM start={hx(a)} end={hx(b)} explicit-pages={hx(e['explicit_pages'])} gap-pages={hx(e['gap_pages'])}"]
    for kind,key in [('RESERVED','reserved'),('CANDIDATE','candidates')]:
        out.extend(f'STYLEOS S827 {kind} {hx(a)} {hx(b)}' for a,b in e[key])
    return out+[POLICY]

def verify(raw,rc,mode):
    ok,d=parent(raw,rc,mode)
    if not ok:return False,{}
    try:
        e=expected(d)
        lines=raw.decode('ascii').replace('\r','').splitlines();rows=[x for x in lines if x.startswith('STYLEOS S827 ')]
        need(rows==records(e),'native map output differs from independent reconstruction')
        i=lines.index(rows[0]);need(lines[i:i+len(rows)]==rows,'noncontiguous proof')
        need(i==1+lines.index('STYLEOS S826 POLICY observation-only=yes ecam=not-read dma-grants=no production-cr3=deny') and i+len(rows)<=lines.index('STYLEOS S823 PASS'),'record ordering')
        d.update(s827_native_detached_map=True,s827_exclusion=e,s827_candidate_grants=False,s827_ecam_access_authorized=False,s827_dma_authorized=False,s827_production_cr3_authorized=False)
        return True,d
    except (ValueError,TypeError,IndexError,KeyError,StopIteration):return False,{}
