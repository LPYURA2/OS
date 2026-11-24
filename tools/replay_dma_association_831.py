#!/usr/bin/env python3
"""Replay fresh S830 raw evidence and derive diagnostic S831 association."""
import argparse, hashlib, json
from pathlib import Path
from irq_sources_830_verify import verify, BACKEND, need
from dma_association_831 import association

def sha(b):return hashlib.sha256(b).hexdigest()

def replay(folder):
    data=json.loads((folder/'results.json').read_text())
    need(data['stage']==830 and data['status']=='PASS' and set(data['modes'])==set(BACKEND),'stage/mode set')
    for key in ('dma_access_authorized','ecam_reservation_verified','ecam_registers_read','production_cr3_authorized','foundation_complete','operationregion_ever_granted','general_iommu_ownership_verified','dma_domains_verified','dma_isolation_verified'):
        need(data[key] is False,'unexpected parent grant '+key)
    modes={}
    for mode in BACKEND:
        row=data['modes'][mode];p=folder/mode
        if mode=='amd' and row['status']=='SKIPPED':
            need(not data['amd_qemu_supported'] and not data['amd_hardware_verified'],'unjustified AMD skip')
            modes[mode]={'status':'SKIPPED'};continue
        need(row['status']=='PASS' and row['qemu_exit_code']==33,'QEMU status '+mode)
        uart=(p/'serial.log').read_bytes();need(sha(uart)==row['uart_sha256'],'UART hash '+mode)
        ok,d=verify(uart,33,mode);need(ok,'S830 replay '+mode)
        dsdt=d.pop('_original_dsdt_bytes');need(dsdt==(p/'dsdt.aml').read_bytes(),'DSDT mismatch')
        for i,b in enumerate(d.pop('_original_ssdt_bytes')):need(b==(p/f'ssdt-{i}.aml').read_bytes(),'SSDT mismatch')
        for key in ('_s824_raw','_s826_raw','_s828_raw','_s830_raw'):
            for name,b in d.pop(key).items():need(b==(p/name).read_bytes(),'raw mismatch '+name)
        d['dsdt_sha256']=sha(dsdt);need(d==row['observations'],'observations mismatch '+mode)
        report=association(uart,33,mode)
        need(all(report[k] is False for k in ('ownership','mmio_authorized','dma_authorized','interrupt_remapping_authorized','production_cr3_authorized','foundation_complete')),'unexpected authority')
        need(all(v[k] is False for v in report['requesters'] for k in ('ownership','mmio_authorized','dma_authorized','interrupt_remapping_authorized')),'requester grant')
        modes[mode]={'status':'PASS','association':report}
        print(mode,{s:sum(v['state']==s for v in report['requesters']) for s in sorted({v['state'] for v in report['requesters']})},'retained',report['retained_blockers'],flush=True)
    return dict(stage=831,status='PASS',parent_stage=830,same_boot_replay=True,new_hardware_execution=False,modes=modes,ownership=False,mmio_authorized=False,dma_authorized=False,interrupt_remapping_authorized=False,production_cr3_authorized=False,foundation_complete=False)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    r=replay(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n')
if __name__=='__main__':main()
