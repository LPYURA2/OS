"""Host-only, bounded first-page census. Never grants IOMMU ownership or MMIO."""
import hashlib
from uefi_map_826 import audit, require
from irq_sources_830_verify import verify, BACKEND
from dma_association_831 import association
from test_acpi_windows_825 import reconcile
PAGE=4096

def overlaps(a,b): return a[0]<b[1] and b[0]<a[1]

def census(raw_map,stride,version,units,ecam,windows,ioapics,candidates,reserved):
    require(0<=len(units)<=16,'unit count')
    # The first page is only a conservative diagnostic probe, not the full aperture.
    spans=[]; reports=[]
    for u in units:
        base=u['base'];require(type(base) is int and 0<base<=2**64-PAGE and base%PAGE==0 and u['segment']==0,'unit base/segment')
        span=[base,base+PAGE];require(not any(overlaps(span,s) for s in spans),'duplicate unit page');spans.append(span)
        m=audit(raw_map,stride,version,*span)
        hits=[]
        for label,ranges in [('ECAM',[ecam]),('PCI_PRODUCER',windows),('IOAPIC',ioapics),('RAM_CANDIDATE',candidates)]:
            if any(overlaps(span,r) for r in ranges):hits.append(label)
        status='CONFLICT' if hits or m['conflicts'] else 'GAP' if m['gaps'] else 'EXPLICIT_NON_RAM_COVERAGE'
        reports.append(dict(unit=u,first_page=span,status=status,conflicts=hits,map_coverage=m['covered'],map_gaps=m['gaps'],map_conflicts=m['conflicts'],reserved_in_detached_map=any(a<=base and base+PAGE<=b for a,b in reserved),full_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False))
    # Validate even absent-IOMMU map input (otherwise an empty units array hides corruption).
    if not units:
        from uefi_map_826 import parse
        parse(raw_map,stride,version)
    return reports

def replay(raw,rc,mode):
    ok,d=verify(raw,rc,mode);require(ok,'S830 replay refused')
    a=association(raw,rc,mode)
    require(not any(a[k] for k in ('ownership','mmio_authorized','dma_authorized','interrupt_remapping_authorized','production_cr3_authorized')),'parent authority')
    m=d['s826_map_audit'];e=d['s827_exclusion'];w=reconcile(d)
    units=d['s828_inventory']['units'];b=BACKEND[mode]
    require((not units)==(b==0),'backend/units')
    require(d['s828_inventory']['distinct_unit_identities']==len(units),'duplicate unit forms unresolved')
    r=census(d['_s826_raw']['final-map.bin'],m['descriptor_size'],m['descriptor_version'],units,w['ecam_interval'],[v[1:] for v in w['producer_windows'] if v[0]==0]+[[v['start'],v['end']] for v in w['allocated_windows'] if v['kind']==0],[[v['address'],v['address']+PAGE] for v in d['s830_ioapics']],e['candidates'],e['reserved'])
    return dict(mode=mode,uart_sha256=hashlib.sha256(raw).hexdigest(),table_sha256=d['s828_table_sha256'],map_sha256=m['map_sha256'],units=r,association=a,full_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)
