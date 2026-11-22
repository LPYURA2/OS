#!/usr/bin/env python3
"""Strict S805 UART validator. Observation != interrupt delivery proof."""
import re
FIELDS = {
    'CPU': ('cpuid-ecx','cpuid-edx','apic-base-msr','lapic-physical','apic-global','x2-mode','bsp'),
    'INTERRUPTS': ('idtr-base','idtr-limit','rflags','if-enabled','pic-master','pic-slave','cr3'),
}
POLICY=b'STYLEOS S805 POLICY cpu-apic-snapshot=yes idtr-observed=yes pic-mask-observed=yes madt-unverified=yes ioapic-routing-unverified=yes irq-delivery-unverified=yes dma-iommu-unverified=yes firmware-runtime-unverified=yes production-cr3=deny'
RX=re.compile(rb'([a-z0-9-]+)=0x([0-9a-f]{16})')
def parse(row,kind):
    prefix=b'STYLEOS S805 '+kind.encode()+b' '
    if not row.startswith(prefix):raise ValueError('prefix')
    body=row[len(prefix):]
    tokens=RX.findall(body)
    if tuple(k.decode() for k,v in tokens)!=FIELDS[kind] or body != b' '.join(k+b'=0x'+v for k,v in tokens):
        raise ValueError('wrong fields/schema')
    return {k.decode():int(v,16) for k,v in tokens}
def verify(data,rc):
    try:
        if not isinstance(data,bytes) or len(data)>500000 or rc!=33:raise ValueError('real QEMU exit 33 required')
        lines=data.splitlines()
        if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:
            raise ValueError('handoff marker')
        records=[v for v in lines if v.startswith(b'STYLEOS S805 ')]
        if len(records)!=4 or records[2:]!=[POLICY,b'STYLEOS S805 PASS']:
            raise ValueError('exact observation/policy/PASS required')
        if lines.index(b'STYLEOS x86_64 ExitBootServices OK')>=lines.index(records[0]):
            raise ValueError('post-EBS observation order')
        if any(b'STYLEOS S805 FAIL' in x for x in lines):raise ValueError('observed failure')
        c=parse(records[0],'CPU');i=parse(records[1],'INTERRUPTS')
        if not c['cpuid-edx']&(1<<9) or not c['cpuid-edx']&(1<<5):raise ValueError('CPU APIC/MSR unavailable')
        if c['apic-global']!=(c['apic-base-msr']>>11)&1 or c['apic-global']!=1:
            raise ValueError('APIC enabled mismatch')
        if c['x2-mode']!=(c['apic-base-msr']>>10)&1:
            raise ValueError('x2 mode mismatch')
        if c['x2-mode'] and not c['cpuid-ecx']&(1<<21):raise ValueError('x2 unsupported')
        if c['bsp']!=(c['apic-base-msr']>>8)&1:raise ValueError('BSP mismatch')
        if c['lapic-physical']!=(c['apic-base-msr']&0x000ffffffffff000) or c['lapic-physical']!=0xfee00000:
            raise ValueError('LAPIC base is not expected fixed Q35 baseline')
        if not i['idtr-base'] or not 15<=i['idtr-limit']<=65535:
            raise ValueError('IDT descriptor invalid')
        if i['if-enabled'] != ((i['rflags']>>9)&1) or i['if-enabled']:
            raise ValueError('IF unexpectedly enabled')
        if i['pic-master']>255 or i['pic-slave']>255 or not i['cr3']&~4095:
            raise ValueError('PIC or CR3 invalid')
        return True,{'cpu':c,'interrupts':i,'irq_delivery_proven':False,
                     'madt_verified':False,'ioapic_routing_verified':False,
                     'dma_iommu_verified':False,'uefi_runtime_verified':False,
                     'production_cr3_authorized':False}
    except (ValueError,IndexError,TypeError,KeyError) as e:
        return False,str(e)
