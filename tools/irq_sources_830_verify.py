"""Strict same-boot MADT correlation; preserve S829 deny-only plan."""
import hashlib,re
from iommu_evidence_828_verify import verify as parent,BACKEND,need
from irq_sources_830 import inspect,madt_ioapics
from requester_plan_829 import plan
POLICY='STYLEOS S830 POLICY observation-only=yes interrupt-remapping=no dma-grants=no production-cr3=deny'

def verify(raw,rc,mode):
    ok,d=parent(raw,rc,mode)
    if not ok:return False,{}
    try:
        lines=raw.decode('ascii').replace('\r','').splitlines()
        rows=[x for x in lines if x.startswith('STYLEOS S830 ')]
        need(len(rows)==2 and rows[1]==POLICY,'S830 records')
        m=re.fullmatch(r'STYLEOS S830 MADT data=([0-9a-f]{88,8192})',rows[0]);need(m,'MADT hex')
        i=lines.index(rows[0]);need(lines[i:i+2]==rows and lines[i+2].startswith('STYLEOS S824 MCFG data=') and lines.index('STYLEOS S823 PASS')<i,'S830 ordering')
        blob=bytes.fromhex(m[1]);apics=madt_ioapics(blob)
        table=d['_s828_raw']['iommu-table.bin'];backend=BACKEND[mode]
        report=inspect(table,backend,d['s828_topology'],blob)
        old=plan(table,backend,d['s828_topology'])
        need(not any(old[k] for k in ('ownership','mmio_authorized','dma_authorized','production_cr3_authorized')),'parent grant')
        count=len(report['special_sources']);need(count<=report['parent_unresolved'],'classification count')
        report['same_boot_madt_proven']=True
        report['remaining_unclassified']=report['parent_unresolved']-count
        d.update(s830_madt_sha256=hashlib.sha256(blob).hexdigest(),s830_ioapics=[{'id':k,**v} for k,v in sorted(apics.items())],s830_irq_inventory=report,s830_unchanged_requester_plan=old,s830_interrupt_remapping_authorized=False,s830_dma_authorized=False,_s830_raw={'madt.bin':blob})
        return True,d
    except (ValueError,KeyError,TypeError,IndexError,StopIteration):return False,{}
