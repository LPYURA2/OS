#!/usr/bin/env python3
"""Replay original same-boot S828 evidence from a directory, never grant access."""
import argparse, hashlib, json
from pathlib import Path
from iommu_evidence_828_verify import verify, BACKEND, need
from requester_plan_829 import plan

def digest(raw): return hashlib.sha256(raw).hexdigest()

def replay(folder):
    data=json.loads((folder/'results.json').read_text())
    need(data['stage']==828 and data['status']=='PASS' and set(data['modes'])==set(BACKEND),'stage/mode set')
    for k in ('dma_access_authorized','ecam_reservation_verified','ecam_registers_read','production_cr3_authorized','foundation_complete','operationregion_ever_granted','general_iommu_ownership_verified','dma_domains_verified','dma_isolation_verified'):
        need(data[k] is False,'unexpected parent grant '+k)
    modes={}
    for mode,backend in BACKEND.items():
        row=data['modes'][mode];p=folder/mode
        if mode=='amd' and row['status']=='SKIPPED':
            need(not data['amd_qemu_supported'] and not data['amd_hardware_verified'],'unjustified AMD skip')
            modes[mode]={'status':'SKIPPED'};continue
        need(row['status']=='PASS' and row['qemu_exit_code']==33,'QEMU status '+mode)
        uart=(p/'serial.log').read_bytes();need(digest(uart)==row['uart_sha256'],'UART digest '+mode)
        ok,d=verify(uart,33,mode);need(ok,'parent replay refused '+mode)
        dsdt=d.pop('_original_dsdt_bytes');need(dsdt==(p/'dsdt.aml').read_bytes(),'DSDT mismatch')
        for i,b in enumerate(d.pop('_original_ssdt_bytes')):need(b==(p/f'ssdt-{i}.aml').read_bytes(),'SSDT mismatch')
        table=d['_s828_raw']['iommu-table.bin']
        for key in ('_s824_raw','_s826_raw','_s828_raw'):
            for name,b in d.pop(key).items():need(b==(p/name).read_bytes(),'raw resource mismatch '+name)
        d['dsdt_sha256']=digest(dsdt)
        need(d==row['observations'],'observations mismatch '+mode)
        need(d['s828_unit_ownership'] is False and d['s828_dma_authorized'] is False,'unexpected inventory grant')
        report=plan(table,backend,d['s828_topology'])
        modes[mode]={'status':'PASS','table_sha256':digest(table),'uart_sha256':digest(uart),'plan':report}
        print(mode, {s:sum(v['state']==s for v in report['requesters']) for s in sorted({v['state'] for v in report['requesters']})}, 'blockers',report['blockers'],flush=True)
    return {'stage':829,'status':'PASS','parent_stage':828,'same_boot_replay':True,'new_hardware_execution':False,'modes':modes,'ownership':False,'mmio_authorized':False,'dma_authorized':False,'production_cr3_authorized':False,'foundation_complete':False}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    result=replay(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
