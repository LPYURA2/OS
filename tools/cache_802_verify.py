#!/usr/bin/env python3
"""Stage 802: reconcile independently captured S800 MTRR and S801 PAT/PTE UART.

Only bounded sample addresses are correlated. The two QEMU boots are not one
atomic processor snapshot; no global alias, production CR3 or DMA claim.
"""
from cache_800_verify import verify as verify_mtrr
from cache_801_verify import verify as verify_pte

EXPECTED=(0x80000000,0x81000000,0xC000000000)


def reconcile(mtrr_uart:bytes,pte_uart:bytes,mtrr_exit:int,pte_exit:int):
    ok,physical=verify_mtrr(mtrr_uart,mtrr_exit)
    if not ok:return False,'S800: '+str(physical)
    ok,virtual=verify_pte(pte_uart,pte_exit)
    if not ok:return False,'S801: '+str(virtual)
    x=physical['cpu']; y=virtual['cpu']; p=physical['pci']
    if (x['cr0'],x['cr3'],x['pat'],x['physbits'])!=(y['cr0'],y['cr3'],y['pat'],y['physbits']):
        return False,'CPU evidence differs between independent boots'
    if not ((x['mtrrdef']&0xc00)==0xc00):
        return False,'MTRR fixed/enabled state not closed'
    physically=(p['gop'],p['bar1raw']&~15,((p['bar5raw']<<32)|(p['bar4raw']&~15)))
    if physically!=EXPECTED:
        return False,'unexpected PCI/GOP device layout'
    rows=[]
    for i,(a,b) in enumerate(zip(physical['targets'],virtual['targets'])):
        if (a['index'],a['address'])!=(i,EXPECTED[i]):return False,'MTRR sample mismatch'
        if (b['index'],b['address'])!=(i,EXPECTED[i]):return False,'PTE sample mismatch'
        if not b['present'] or b['translated']!=EXPECTED[i]:
            return False,'device identity translation missing at sampled virtual address'
        if b['page-size'] not in (4096,2097152,1073741824):
            return False,'invalid page size'
        if a['mtrr-type']!=0 or a['enabled']!=1:
            return False,'physical UC MTRR cannot be proven for sampled device'
        if b['pat-type']!=6:
            return False,'unexpected PAT leaf selection; manual analysis required'
        if not (b['entry']&1):return False,'PTE not present'
        rows.append({'device':('GOP','VirtIO-BAR1','VirtIO-BAR4')[i],
            'sample_virtual_address':EXPECTED[i], 'translated_physical_address':b['translated'],
            'mtrr_type':'UC','selected_pat_type':'WB','derived_sample_type':'UC',
            'page_size':b['page-size'],'pat_index':b['pat-index'],
            'scope':'sampled-cross-boot-only'})
    return True,{'stage':802,'status':'PASS','sample_count':3,'samples':rows,
       'observations_correlated_across_independent_qemu_boots':True,
       'derived_cache_type_for_samples_only':True,
       'same_boot_atomic_measurement':False,
       'global_mmio_alias_scan_verified':False,
       'all_mmio_cache_types_verified':False,
       'irq_apic_verified':False,'dma_iommu_verified':False,
       'uefi_runtime_verified':False,'production_cr3_authorized':False}
