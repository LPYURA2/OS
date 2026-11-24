"""Bounded special I/O APIC source inventory; not interrupt-remapping enablement.
S829 remains unchanged: this report cannot remove blockers or grant ownership.
MADT must come from the same boot; caller must establish provenance separately.
"""
import hashlib
from iommu_evidence_828_verify import decode, topology, le, need


def madt_ioapics(raw):
    need(44 <= len(raw) <= 4096 and raw[:4] == b'APIC' and le(raw[4:8]) == len(raw) and sum(raw)%256 == 0, 'bad MADT')
    p=44; out={}; bases=set(); gsis=set()
    while p<len(raw):
        need(p+2<=len(raw), 'MADT entry header'); k,n=raw[p:p+2]
        need(2<=n<=len(raw)-p,'MADT entry length'); e=raw[p:p+n];p+=n
        if k==1:
            need(n==12 and e[3]==0 and len(out)<8,'IOAPIC shape')
            id=e[2]; base=le(e[4:8]);gsi=le(e[8:12])
            need(base and base%4096==0 and id not in out and base not in bases and gsi not in gsis,'IOAPIC duplicate/extent')
            out[id]={'address':base,'gsi_base':gsi};bases.add(base);gsis.add(gsi)
    need(out,'missing MADT IOAPIC'); return out


def inspect(raw,backend,rows,madt=None):
    funcs,buses=topology(rows); inv=decode(raw,backend,funcs,buses)
    apics=madt_ioapics(madt) if madt is not None else None
    special=[]; other=[]; pos=48; unit=0
    direct={r for r,_,_ in inv['exact']}; aliases={r for r,_,_ in inv['aliases']}; targets={r for _,r,_ in inv['aliases']}
    def add(offset, record, ioid, rid, segment, valid):
        need(valid and segment==0,'unsupported IOAPIC encoding')
        need(not any(x['ioapic_id']==ioid or x['source_rid']==rid for x in special),'duplicate special IOAPIC identity/source')
        # Same-ID source can be shared with real PCI on other machines. This
        # narrow preflight refuses it rather than assuming source isolation.
        need(rid not in funcs and rid not in direct and rid not in aliases and rid not in targets,'IRQ source collides with PCI claims')
        if apics is not None: need(ioid in apics,'IOAPIC absent from MADT')
        special.append(dict(offset=offset,record_hex=record.hex(),unit=unit,segment=segment,ioapic_id=ioid,source_rid=rid,
                           classification='IOAPIC_INTERRUPT_SOURCE',madt_id_matched=apics is not None,
                           madt=apics.get(ioid) if apics is not None else None))
    while backend and pos<len(raw):
        k=le(raw[pos:pos+2]) if backend==1 else raw[pos];n=le(raw[pos+2:pos+4]);v=raw[pos:pos+n]
        if backend==1 and k==0:
            s=16
            while s<n:
                t,l=v[s:s+2];q=v[s:s+l]
                if t==3:
                    # Only direct single-path source ID and zero reserved bytes.
                    need(l==8,'unsupported multi-hop IOAPIC path')
                    add(pos+s,q,q[4],(q[5]<<8)|(q[6]<<3)|q[7],le(v[6:8]),bool(raw[37]&1) and q[2:4]==b'\0\0' and q[6]<=31 and q[7]<=7)
                elif t not in (1,2):other.append({'offset':pos+s,'type':t})
                s+=l
            unit+=1
        elif backend==2 and k in (0x10,0x11,0x40):
            s=24 if k==0x10 else 40
            while s<n:
                t=v[s]
                if t==0xf0:other.append({'offset':pos+s,'type':t});break
                l=16 if t&0x80 else 8 if t&0x40 else 4;q=v[s:s+l]
                if t==0x48:
                    need(l==8,'special entry length')
                    add(pos+s,q,q[4],le(q[5:7]),le(v[16:18]),q[1:4]==b'\0\0\0' and q[7]==1)
                elif t not in (1,2,3,4,0x42,0x43,0x46,0x47):other.append({'offset':pos+s,'type':t})
                s+=l
            unit+=1
        pos+=n
    if backend and apics is not None:
        need(set(apics)=={x['ioapic_id'] for x in special},'incomplete MADT IOAPIC correlation')
    return dict(backend=backend, table_sha256=hashlib.sha256(raw).hexdigest(),special_sources=special,other_entries=other,
                parent_unresolved=inv['unresolved'],madt_supplied=madt is not None,same_boot_madt_proven=False,
                blockers_removed=False,ownership=False,interrupt_remapping_authorized=False,dma_authorized=False,production_cr3_authorized=False)
