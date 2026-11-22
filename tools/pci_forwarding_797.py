"""Stage 797, fail-closed PCI bridge forwarding audit of real S796 UART.

A BAR base inside a bridge window does NOT prove its entire aperture fits.
This is postprocessing of same-run QEMU evidence, not a new EFI or driver.
"""
from collections import defaultdict
from pci_topology_796_verify import FIELDS, TOKENS, verify

class ForwardingError(ValueError):
    pass

def _records(raw, label):
    prefix=b'STYLEOS S796 '+label.encode()+b' '
    out=[]
    for line in raw.splitlines():
        if not line.startswith(prefix):continue
        fields=TOKENS.findall(line[len(prefix):])
        if len(fields)!=len(FIELDS[label]) or tuple(k.decode() for k,_ in fields)!=FIELDS[label]:
            raise ForwardingError('malformed '+label)
        out.append({k.decode():int(v,16) for k,v in fields})
    return out

def audit(raw, exit_code, *, require_bridge):
    valid,reason=verify(raw,exit_code,require_bridge=require_bridge)
    if not valid:raise ForwardingError('S796 hardware attestation: '+str(reason))
    bridges=_records(raw,'BRIDGE')
    devices=_records(raw,'DEVICE')
    bars=_records(raw,'BAR')
    gop=_records(raw,'GOP')[0]
    stats=_records(raw,'INVENTORY')[0]
    parents={}
    for b in bridges:
        sec=b['secondary']
        if sec in parents or sec==b['primary'] or sec==0:
            raise ForwardingError('ambiguous route')
        parents[sec]=b
    for bus in parents:
        seen=set()
        while bus:
            if bus in seen or bus not in parents:
                raise ForwardingError('cyclic/missing route')
            seen.add(bus)
            bus=parents[bus]['primary']
    routed_devices=sum(d['bus']>0 for d in devices)
    routed_memory=0
    unverified_io=0
    by_bus=defaultdict(int)
    for bar in bars:
        bus=bar['bus']
        if not bus:continue
        if not bar['memory']:
            unverified_io+=1
            continue
        current=bus
        while current:
            b=parents.get(current)
            if b is None:raise ForwardingError('missing bridge')
            if bar['prefetch']:
                start,end=b['prefetch-base'],b['prefetch-end']
            else:
                start,end=b['memory-base'],b['memory-end']
            if not (start>0 and end>start and start<=bar['base']<end):
                raise ForwardingError('BAR base outside bridge window')
            current=b['primary']
        routed_memory+=1
        by_bus[bus]+=1
    if require_bridge and (not routed_devices or not routed_memory):
        raise ForwardingError('empty bridged QEMU evidence')
    if len(parents)!=stats['bridges'] or len(bars)!=stats['bar-sizes-unknown']:
        raise ForwardingError('BAR inventory mismatch')
    if not gop['base']:raise ForwardingError('GOP base missing')
    return {
        'buses':stats['buses'],'bridges':stats['bridges'],
        'devices':stats['devices'],'total_bars':len(bars),
        'routed_devices':routed_devices,
        'routed_memory_bar_bases_in_bridge_windows':routed_memory,
        'routed_io_bar_bases_not_assessed':unverified_io,
        'routed_memory_by_bus':dict(sorted(by_bus.items())),
        'all_bar_aperture_sizes_verified':False,
        'bridge_forwarding_proved_for_entire_bars':False,
        'irq_routing_verified':False,
        'effective_cache_type_verified':False,
        'production_cr3_authorized':False
    }
