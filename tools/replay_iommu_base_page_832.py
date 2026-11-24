#!/usr/bin/env python3
"""Recheck S831 report and same-boot S830 evidence before first-page census."""
import argparse,json
from pathlib import Path
from replay_dma_association_831 import replay as prior
from iommu_base_page_832 import replay,require


def run(folder,association_file):
    previous=prior(folder)
    require(previous==json.loads(association_file.read_text()),'S831 replay differs')
    modes={}
    for mode,v in previous['modes'].items():
        if v['status']=='SKIPPED':modes[mode]={'status':'SKIPPED'};continue
        r=replay((folder/mode/'serial.log').read_bytes(),33,mode)
        require(r['association']==v['association'],'association mismatch')
        require(not any(r[k] for k in ('full_aperture_verified','ownership','mmio_authorized','dma_authorized','production_cr3_authorized')),'unexpected authority')
        require(all(not u[k] for u in r['units'] for k in ('full_aperture_verified','ownership','mmio_authorized','dma_authorized')),'unit grant')
        modes[mode]={'status':'PASS','census':r}
        print(mode,[(hex(u['unit']['base']),u['status'],u['conflicts']) for u in r['units']],flush=True)
    return dict(stage=832,status='PASS',parent_stage=831,new_hardware_execution=False,modes=modes,full_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False,foundation_complete=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--association',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    r=run(a.input,a.association);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n')
