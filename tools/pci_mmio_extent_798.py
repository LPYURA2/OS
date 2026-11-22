"""Fail-closed, cross-evidence MMIO range census; NOT production mapping authority.

795 measured one VGA BAR in its own QEMU process; 796 enumerated a separate
baseline and bridged QEMU. A matching BDF/base does not turn the latter's BAR
size into a measurement. Unknown extents remain explicitly unknown.
"""
import hashlib
from pci_aperture_795_verify import verify as verify795, TOKENS as TOKENS795
from pci_forwarding_797 import audit as audit797, _records, ForwardingError

class ExtentError(ValueError):
    pass


def validated_measurement(serial, result):
    ok, parsed=verify795(serial,33)
    if not ok:
        raise ExtentError('invalid S795 hardware UART: '+str(parsed))
    if (result.get('stage') != 795 or result.get('status') != 'PASS'
        or result.get('qemu_exit_code') != 33
        or result.get('bar_aperture_size_verified') is not True
        or result.get('bar_probe_restored') is not True
        or result.get('framebuffer_aperture_covered') is not True
        or result.get('production_cr3_authorized') is not False
        or result.get('uart_sha256') != hashlib.sha256(serial).hexdigest()):
        raise ExtentError('S795 hardware provenance mismatch')
    return parsed


def inspect(aperture_log, aperture_result, topologies, topology_results,
            forwarding_report):
    """Inspect exactly 3 separately executed QEMU UARTs + S797 attestation."""
    measured=validated_measurement(aperture_log, aperture_result)
    result={'stage':798,'status':'PASS','source':'same-CI-three-QEMU-hardware-logs',
            'verified_apertures':[],'topologies':{},
            'all_mmio_apertures_verified':False,'mmio_mapping_authorized':False,
            'effective_cache_type_verified':False,'irq_routing_verified':False,
            'uefi_runtime_closure_verified':False,'production_cr3_authorized':False}
    measured_pci=measured['PCI'];measured_gop=measured['GOP'];s=measured['SIZING']
    if (s['end'] > 1<<64 or s['end'] <= measured_pci['base'] or
        measured_pci['base']+measured_gop['bytes']>s['end']):
        raise ExtentError('invalid measured aperture bounds')
    result['verified_apertures']=[{'source':'S795-QEMU-measured-and-restored',
        'device': [measured_pci[x] for x in ('bus','device','function')],
        'vendor':measured_pci['vendor'],'id':measured_pci['id'],
        'index':measured_pci['index'],'base':measured_pci['base'],
        'end_exclusive':s['end'],'size_bytes':s['size'],
        'uart_sha256':hashlib.sha256(aperture_log).hexdigest()}]
    if (forwarding_report.get('stage') != 797 or
        forwarding_report.get('status') != 'PASS' or
        forwarding_report.get('source') != 'same-run-hardware-S796-QEMU' or
        forwarding_report.get('production_cr3_authorized') is not False):
        raise ExtentError('missing S797 attestation')
    obs=forwarding_report.get('observations')
    if not isinstance(obs,dict) or set(obs)!={'default','bridge'}:
        raise ExtentError('incomplete S797 observation set')
    for label in ('default','bridge'):
        raw=topologies.get(label)
        report=topology_results.get(label)
        if not isinstance(raw,bytes) or not isinstance(report,dict):
            raise ExtentError('missing same-run S796 evidence '+label)
        digest=hashlib.sha256(raw).hexdigest()
        if (report.get('stage')!=796 or report.get('status')!='PASS' or
            report.get('qemu_exit_code')!=33 or report.get('scenario')!=label or
            report.get('uart_sha256')!=digest or
            report.get('production_cr3_authorized') is not False or
            report.get('unknown_bar_sizes_remain') is not True):
            raise ExtentError('invalid S796 hardware provenance '+label)
        try: validated=audit797(raw,33,require_bridge=(label=='bridge'))
        except ForwardingError as ex: raise ExtentError('S797 failed '+label+': '+str(ex)) from ex
        prior=obs[label]
        if prior.get('serial_sha256')!=digest or any(
                prior.get(k)!=({str(i):count for i,count in v.items()} if k=='routed_memory_by_bus' else v)
                for k,v in validated.items()):
            raise ExtentError('S797 not bound to same hardware/QEMU '+label)
        devices=_records(raw,'DEVICE')
        bars=_records(raw,'BAR')
        bridges=_records(raw,'BRIDGE')
        gop=_records(raw,'GOP')[0]
        identity=(measured_pci['bus'],measured_pci['device'],measured_pci['function'])
        matches=[d for d in devices if (d['bus'],d['device'],d['function'])==identity]
        matching_bars=[b for b in bars if
                (b['bus'],b['device'],b['function'],b['index'])==(*identity,measured_pci['index'])]
        if (len(matches)!=1 or len(matching_bars)!=1 or
            matches[0]['vendor']!=measured_pci['vendor'] or
            matches[0]['id']!=measured_pci['id'] or
            not matching_bars[0]['memory'] or
            matching_bars[0]['base']!=measured_pci['base'] or
            gop['base']!=measured_pci['base'] or
            gop['bytes']!=measured_gop['bytes'] or
            (gop['bus'],gop['device'],gop['function'])!=identity):
            raise ExtentError('GOP BAR identity/base mismatch '+label)
        # Even another device's BAR *base* inside measured VGA space would be
        # ambiguous ownership; refuse rather than silently bless it.
        for b in bars:
            if b is not matching_bars[0] and b['memory'] and measured_pci['base']<=b['base']<s['end']:
                raise ExtentError('another BAR base overlaps measured VGA range '+label)
        parents={b['secondary']:b for b in bridges}
        unknown=[]
        for bar in bars:
            addr=bar['base'];cursor=bar['bus'];bounds=[]
            if bar['memory']:
                while cursor:
                    parent=parents.get(cursor)
                    if not parent:raise ExtentError('missing ancestor')
                    begin,end=(parent['prefetch-base'],parent['prefetch-end']) if bar['prefetch'] else (parent['memory-base'],parent['memory-end'])
                    if not begin<=addr<end:raise ExtentError('BAR outside forwarding window')
                    bounds.append(end)
                    cursor=parent['primary']
            unknown.append({'device':[bar[x] for x in ('bus','device','function')],
                'index':bar['index'],'memory':bool(bar['memory']),
                'prefetch':bool(bar['prefetch']),'base':addr,
                'size_bytes':None,'end_exclusive':None,
                'forwarding_window_end_exclusive':min(bounds) if bounds else None,
                'maximum_possible_forwarded_bytes': min(bounds)-addr if bounds else None,
                'range_authorized':False})
        result['topologies'][label]={'uart_sha256':digest,
            'bar_count':len(bars),'bridge_count':len(bridges),
            'all_bar_sizes_unknown_in_this_qemu':True,
            'gop_bar_identity_corresponds_to_s795':True,
            's795_measurement_applies_to_this_boot':False,
            'resources_without_full_aperture_attestation':unknown}
    return result
