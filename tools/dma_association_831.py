"""Deny-only refinement of S829 after S830 same-boot verification.
Input is the original S830 UART, not caller-supplied summary or altered ACPI.
Association is informational: even associated controllers/devices never gain DMA.
"""
from irq_sources_830_verify import verify
from iommu_evidence_828_verify import BACKEND, need
from requester_plan_829 import plan
from irq_sources_830 import inspect
import hashlib


def refine_blockers(old,classified,remaining,other_entries):
    blockers=list(old['blockers'])
    removed=[]
    # Retain every other blocker. Only the single known generic blocker is
    # refined when *all* unresolved records are classified without leftovers.
    if classified and remaining==0 and not other_entries and 'unresolved-records' in blockers:
        blockers.remove('unresolved-records');removed.append('unresolved-records')
    return blockers,removed


def association(raw, rc, mode):
    ok,d=verify(raw,rc,mode)
    need(ok,'S830 verification refused')
    table=d['_s828_raw']['iommu-table.bin']
    madt=d['_s830_raw']['madt.bin']
    backend=BACKEND[mode]
    old=plan(table,backend,d['s828_topology'])
    need(old==d['s830_unchanged_requester_plan'],'S829 plan mismatch')
    r=inspect(table,backend,d['s828_topology'],madt)
    classified=r['special_sources']
    remaining=r['parent_unresolved']-len(classified)
    need(remaining>=0 and d['s830_irq_inventory']['remaining_unclassified']==remaining,'classification accounting')
    need(d['s830_irq_inventory']['same_boot_madt_proven'] is True,'missing same boot evidence')
    need(not any(r[k] for k in ('blockers_removed','ownership','interrupt_remapping_authorized','dma_authorized','production_cr3_authorized')),'unexpected authority')
    blockers,removed=refine_blockers(old,classified,remaining,r['other_entries'])
    rows=[]
    unit_requesters={u['requester'] for u in d['s828_inventory']['units'] if u['requester'] is not None}
    live={rid:kind for rid,kind,_,_ in d['s828_topology']}
    for v in old['requesters']:
        need(not v['ownership'] and not v['dma_authorized'],'unexpected requester authority')
        if v['state'] in ('CONFLICT','UNRESOLVED_ALIAS'):state=v['state']
        elif blockers:state='BLOCKED'
        elif len(v['claims'])==1:state='ASSOCIATED'
        elif not v['claims']:state='NO_CLAIM'
        else:state='CONFLICT'
        role='IOMMU_CONTROLLER' if v['rid'] in unit_requesters else 'PCI_BRIDGE' if live[v['rid']]==2 else 'PCI_FUNCTION'
        rows.append(dict(rid=v['rid'],role=role,claims=list(v['claims']),source=list(v['source']),prior_state=v['state'],state=state,ownership=False,mmio_authorized=False,dma_authorized=False,interrupt_remapping_authorized=False))
    return dict(stage=831,mode=mode,uart_sha256=hashlib.sha256(raw).hexdigest(),table_sha256=hashlib.sha256(table).hexdigest(),madt_sha256=hashlib.sha256(madt).hexdigest(),classified=classified,remaining_unclassified=remaining,removed_diagnostic_blockers=removed,retained_blockers=blockers,requesters=rows,ownership=False,mmio_authorized=False,dma_authorized=False,interrupt_remapping_authorized=False,production_cr3_authorized=False,foundation_complete=False)
