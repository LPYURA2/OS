"""Local S829 preflight: proposed controller association, never ownership.
Depends on accepted/under-test S828 bounded decoder. Segment zero only.
Unknown semantics and exception records cause global refusal of candidate
admission. A candidate is diagnostic, not an MMIO or DMA grant.
"""
from iommu_evidence_828_verify import decode, topology, resolve, le, need


def plan(raw, backend, rows):
    funcs, buses = topology(rows)
    inv = decode(raw, backend, funcs, buses)
    blockers = []
    if inv['unresolved']: blockers.append('unresolved-records')
    if inv['exceptions']: blockers.append('memory-exceptions')
    if inv['segments']: blockers.append('unsupported-segments')
    if inv['distinct_unit_identities'] != len(inv['units']): blockers.append('multiple-descriptor-forms')
    # S828 only checked reachable secondary buses and ancestor upper bounds.
    # A sibling range collision makes even a topology-derived candidate unsafe.
    children = {sec:rid for rid,(kind,sec,sub) in funcs.items() if kind==2}
    for sec,rid in children.items():
        parent = rid >> 8
        sub = funcs[rid][2]
        if sec <= parent or not (sec <= sub <= buses[parent]):
            need(False,'non-forward bridge numbering')
        for other,owner in children.items():
            if sec < other and owner>>8 == parent:
                need(sub < other,'overlapping sibling bridge windows')
    # Do not collapse descriptor indices or expand aliases without a validated contract.
    direct = {rid:u for rid,u,_ in inv['exact']}
    alias = {rid:(target,u) for rid,target,u in inv['aliases']}
    if backend == 2:
        if any(src == 3 for _,_,src in inv['exact']): blockers.append('amd-extended-semantics-unresolved')
        if any(u['requester'] not in funcs for u in inv['units']): blockers.append('amd-unit-requester-not-observed')
        if any(target == rid or target in alias or target not in funcs or direct.get(target)!=unit
               for rid,(target,unit) in alias.items()): blockers.append('amd-unresolved-alias-group')
    covers = {rid:set() for rid in funcs}
    defaults = []
    def descends(bus, ancestor):
        seen=set()
        while bus != 0:
            need(bus not in seen,'cycle');seen.add(bus)
            if bus==ancestor:return True
            owners=[rid for rid,(kind,sec,sub) in funcs.items() if kind==2 and sec==bus]
            need(len(owners)==1,'bridge ancestry');bus=owners[0]>>8
        return False
    pos=48;unit=0
    while backend and pos<len(raw):
        typ=le(raw[pos:pos+2]) if backend==1 else raw[pos]
        size=le(raw[pos+2:pos+4]);v=raw[pos:pos+size];pos+=size
        if backend==1 and typ==0:
            if v[4]&1: defaults.append(unit)
            s=16
            while s<size:
                k,n=v[s:s+2]
                if k==2:
                    bridge=resolve(funcs,buses,v[s+5],v[s+6:s+n],2)
                    if bridge is not None:
                        sec=funcs[bridge][1]
                        for rid in funcs:
                            if rid==bridge or descends(rid>>8,sec): covers[rid].add(unit)
                s+=n
            unit+=1
        elif backend==2 and typ in (0x10,0x11,0x40):
            # IVHD data-setting bytes and extended fields are not interpreted
            # as permission. Refuse even candidate admission when present.
            s=24 if typ==0x10 else 40
            while s<size:
                k=v[s]
                if k==0xf0: break  # Parent already flags the opaque tail.
                n=16 if k&0x80 else 8 if k&0x40 else 4
                if v[s+3] or (k in (0x46,0x47) and any(v[s+4:s+n])):
                    if 'amd-device-policy-unresolved' not in blockers:
                        blockers.append('amd-device-policy-unresolved')
                s+=n
            # IVHD all-device claims are deliberately not expanded by this draft.
            unit+=1
    if backend==2 and inv['include_all']: blockers.append('amd-all-device-semantics-unresolved')
    if len(defaults)>1: blockers.append('multiple-include-all')
    report=[]
    for rid in funcs:
        claims=set(covers[rid]);source=[]
        if claims:source.append('bridge-scope')
        if rid in direct:claims.add(direct[rid]);source.append('explicit')
        if rid in alias:
            target,u=alias[rid]
            if target==rid or target in alias or target not in funcs or direct.get(target)!=u:
                source.append('unresolved-alias')
            else:claims.add(u);source.append('same-unit-explicit-alias')
        if not claims and rid not in alias and len(defaults)==1:
            claims.add(defaults[0]);source.append('include-all-fallback')
        if len(claims)>1: state='CONFLICT'
        elif 'unresolved-alias' in source:state='UNRESOLVED_ALIAS'
        elif blockers:state='BLOCKED'
        elif len(claims)==1:state='CANDIDATE'
        else:state='NO_CLAIM'
        report.append(dict(rid=rid,claims=sorted(claims),source=source,state=state,ownership=False,dma_authorized=False))
    return dict(backend=backend,blockers=blockers,requesters=report,unobserved_direct=sorted(set(direct)-set(funcs)),unobserved_alias=sorted(set(alias)-set(funcs)),ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)
