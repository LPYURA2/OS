#!/usr/bin/env python3
"""S825 same-boot resource window containment audit. No MMIO, ECAM or DMA grant."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from acpi_resources_824_verify import verify

MODES = ('disabled', 'intel', 'bridge-intel', 'bridge-two-hop-intel', 'amd')
LIMIT = 1 << 64


def need(ok, why):
    if not ok:
        raise ValueError(why)


def interval(v, uefi=False):
    kind, lo, hi, length = (v[k] for k in ('kind', 'lo', 'hi', 'length'))
    need(kind in (0, 1, 2) and 0 <= lo <= hi < LIMIT, 'invalid interval')
    need(length > 0 and hi-lo+1 == length and v['fixed'] and v['translation'] == 0, 'unresolved interval')
    need(kind != 1 or hi < 65536, 'I/O outside 16-bit domain')
    need(kind != 2 or hi < 256, 'bus outside 8-bit domain')
    if uefi:
        need(v['tag'] == 0x8a and v['flags'] == 0 and v['specific'] == 0, 'unsupported UEFI flags')
        if kind == 0:
            need(v['gran'] in (32,64) and hi < (1 << v['gran']), 'UEFI memory width')
        else:
            need(v['gran'] == 0, 'UEFI bus/I/O granularity')
    else:
        need(v['flags'] == 12 and v['tag'] in (0x88,0x87,0x8a), 'not fixed positive-decode producer')
        need(v['gran'] == 0, 'unsupported AML granularity')
        need(v['specific'] in ({0:(1,3),1:(3,),2:(0,)}[kind]), 'unsupported AML type flags')
    return kind, lo, hi+1


def intersects(a,b):
    return a[0] == b[0] and a[1] < b[2] and b[1] < a[2]


def disjoint(ranges, why):
    for i,a in enumerate(ranges):
        need(not any(intersects(a,b) for b in ranges[:i]), why)


def reconcile(d):
    need(d['s824_live_bus_coverage'] and not d['s824_resource_ownership'], 'parent boundary')
    need(not d['s824_resource_conflicts'] and not d['s824_unresolved_memory_descriptors'], 'parent unresolved memory')
    producers=[]; legacy=[]
    for v in d['s824_crs_descriptors']:
        if v['tag'] == 0x47:
            need(v['kind']==1 and v['flags'] is None and v['fixed'] and v['translation']==0
                 and (v['lo'],v['hi'],v['length'])==(0xcf8,0xcff,8), 'unsupported small I/O consumer')
            legacy.append((1,0xcf8,0xd00))
        else:
            producers.append(interval(v))
    need(len(legacy)==1, 'expected single legacy config I/O claim')
    disjoint(producers+legacy, 'AML window overlap')
    roots=d['s824_uefi_roots']; need(len(roots)==1 and roots[0]['segment']==0,'unsupported root/segment')
    allocated=[interval(v,True) for v in roots[0]['descriptors']]
    disjoint(allocated,'UEFI allocated window overlap')
    need({a[0] for a in allocated}=={0,1,2},'missing resource class')
    matches=[]
    for a in allocated:
        candidates=[p for p in producers if a[0]==p[0] and p[1]<=a[1] and a[2]<=p[2]]
        need(len(candidates)==1 and not any(intersects(a,p) for p in legacy),'UEFI resource outside unique producer window')
        matches.append({'kind':a[0],'start':a[1],'end':a[2],'producer_start':candidates[0][1],'producer_end':candidates[0][2]})
    windows=d['s824_mcfg_windows']; need(len(windows)==1 and windows[0]['segment']==0,'unsupported MCFG segment')
    w=windows[0]; start,end=w['start'],w['end']; first,last=w['first'],w['last']
    need(0<=first<=last<=255 and 0<start<end<=LIMIT and start%(1<<20)==0 and end-start==(last-first+1)*(1<<20),'invalid ECAM extent')
    ecam=(0,start,end)
    need(not any(intersects(ecam,p) for p in producers+allocated),'ECAM overlaps bridge resources')
    live=set(d['s824_live_buses']); need(live and 0 in live and all(0<=b<=255 for b in live),'invalid live bus set')
    need(live=={b for a in allocated if a[0]==2 for b in range(a[1],a[2])},'UEFI bus allocation differs from observed buses')
    need(live<=set(range(first,last+1)),'live bus outside ECAM')
    return {'allocated_windows':matches,'producer_windows':[list(p) for p in producers],
            'legacy_config_io':[0xcf8,0xd00],'ecam_interval':[start,end],
            'ecam_excluded_from_bridge_windows':True,'ecam_reservation_verified':False,
            'live_buses':sorted(live),'resource_ownership':False,'dma_authorized':False,
            'production_cr3_authorized':False}


def replay(folder, mode, record):
    need(record['status']=='PASS' and record['qemu_exit_code']==33,'QEMU proof missing')
    raw=(folder/'serial.log').read_bytes()
    need(hashlib.sha256(raw).hexdigest()==record['uart_sha256'],'UART hash mismatch')
    ok,d=verify(raw,33,mode); need(ok,'parent UART replay refused')
    dsdt=d.pop('_original_dsdt_bytes');need((folder/'dsdt.aml').read_bytes()==dsdt,'DSDT differs')
    for i,b in enumerate(d.pop('_original_ssdt_bytes')):
        need((folder/f'ssdt-{i}.aml').read_bytes()==b,'SSDT differs')
    for name,b in d.pop('_s824_raw').items():
        need((folder/name).read_bytes()==b,'raw resource differs: '+name)
    d['dsdt_sha256']=hashlib.sha256(dsdt).hexdigest()
    need(d==record['observations'],'parent JSON differs from UART')
    return d


def negatives(d):
    cases=[]
    def add(label, fn): cases.append((label,fn))
    def crs(x): return x['s824_crs_descriptors']
    def u(x): return x['s824_uefi_roots'][0]['descriptors']
    def m(x): return x['s824_mcfg_windows'][0]
    add('ownership claim',lambda x:x.update(s824_resource_ownership=True))
    add('missing coverage',lambda x:x.update(s824_live_bus_coverage=False))
    add('producer becomes consumer',lambda x:crs(x)[0].update(flags=13))
    add('subtractive decode',lambda x:crs(x)[0].update(flags=14))
    add('movable window',lambda x:crs(x)[0].update(fixed=False))
    add('translated window',lambda x:u(x)[0].update(translation=1))
    add('unexpected UEFI flags',lambda x:u(x)[0].update(flags=1))
    add('wrong memory width',lambda x:next(v for v in u(x) if v['kind']==0 and v['gran']==64).update(gran=32))
    add('length mismatch',lambda x:u(x)[0].update(length=1))
    add('duplicate producer',lambda x:crs(x).append(copy.deepcopy(crs(x)[0])))
    add('duplicate allocation',lambda x:u(x).append(copy.deepcopy(u(x)[0])))
    add('outside producer',lambda x:u(x)[0].update(lo=0xcf8,hi=0xcff,length=8))
    add('I/O overflow',lambda x:u(x)[0].update(lo=0x10000,hi=0x10fff,length=0x1000))
    add('ECAM overlaps window',lambda x:m(x).update(start=0x80000000,end=0x90000000))
    add('ECAM size mismatch',lambda x:m(x).update(end=m(x)['end']-1))
    add('live bus missing',lambda x:x.update(s824_live_buses=[0,255]))
    add('nonzero segment',lambda x:x['s824_uefi_roots'][0].update(segment=1))
    add('unresolved memory',lambda x:x.update(s824_unresolved_memory_descriptors=1))
    for label,fn in cases:
        x=copy.deepcopy(d);fn(x)
        try: reconcile(x)
        except ValueError: pass
        else: raise AssertionError('accepted '+label)
    return len(cases)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    source=json.loads((args.input/'results.json').read_text());need(source['stage']==824 and source['status']=='PASS','wrong input stage')
    need(set(source['modes'])==set(MODES),'wrong mode set')
    for k in ('dma_access_authorized','ecam_reservation_verified','ecam_registers_read','production_cr3_authorized','foundation_complete'):
        need(source[k] is False,'parent authority claim '+k)
    results={};cases=0
    for mode in MODES:
        record=source['modes'][mode]
        if mode=='amd' and record['status']=='SKIPPED':
            need(not source['amd_qemu_supported'] and not source['amd_hardware_verified'],'unjustified AMD skip');results[mode]={'status':'SKIPPED'};continue
        d=replay(args.input/mode,mode,record);r=reconcile(d)
        if mode=='disabled': cases=negatives(d)
        results[mode]={'status':'PASS',**r};print('S825',mode,'PASS',len(r['allocated_windows']),'allocated windows',flush=True)
    need(cases==18,'negative count')
    result={'stage':825,'status':'PASS','same_boot_824_replay':True,'negative_pass':cases,'modes':results,'ecam_reservation_verified':False,'resource_ownership':False,'dma_authorized':False,'foundation_complete':False}
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('S825 negatives',cases,'PASS; ECAM exclusion is NOT reservation; DMA DENY')


if __name__=='__main__':main()
