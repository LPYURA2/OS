"""Verify native S835 fixture output against independent same-boot QMP and EFI replay."""
import argparse,hashlib,json
from pathlib import Path
from post_ebs_aperture_834 import replay,ungate,run as prior
from irq_sources_830_verify import verify,BACKEND,need
from iommu_aperture_833 import correlate
POLICY='STYLEOS S835 POLICY fixture-only=yes detached-map=yes ram-grants=no mmio=not-read dma-grants=no production-cr3=deny'

def records(raw,rc,mode,trace):
    r=replay(raw,rc,mode,trace);old=ungate(raw);ok,d=verify(old,rc,mode);need(ok,'parent evidence')
    c=correlate(old,rc,mode,trace['initial']);a=c['initial_qemu_model_apertures'];backend=BACKEND[mode]
    if a:
        need(len(a)==1,'one unit');p=a[0];extent=p['interval'];cover=p['denied_page_interval'];explicit=sum((v['intersection'][1]-v['intersection'][0])//4096 for v in p['covered']);gap=(cover[1]-cover[0])//4096-explicit
    else:extent=cover=[0,0];explicit=gap=0
    hx=lambda n:f'0x{n:016x}'
    lines=[f'STYLEOS S835 SUMMARY backend={hx(backend)} explicit-pages={hx(explicit)} gap-pages={hx(gap)}']
    for label,ranges in [('APERTURE',[extent]),('COVER',[cover]),('RESERVED',r['detached_denied']),('CANDIDATE',r['unchanged_candidates'])]:
        lines.extend(f'STYLEOS S835 {label} start={hx(lo)} end={hx(hi)}' for lo,hi in ranges)
    return lines+[POLICY],dict(aperture=extent,cover=cover,explicit_pages=explicit,gap_pages=gap,reserved=r['detached_denied'],candidates=r['unchanged_candidates'],allocator_integrated=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)

def verify_native(raw,rc,mode,trace):
    expected,report=records(raw,rc,mode,trace)
    lines=raw.decode('ascii').replace('\r','').splitlines();actual=[v for v in lines if v.startswith('STYLEOS S835 ')]
    need(actual==expected,'S835 native range mismatch')
    i=lines.index(actual[0]);need(lines[i:i+len(actual)]==actual and i==lines.index('STYLEOS S827 POLICY detached-map=yes ram-grants=no ecam=not-read dma-grants=no production-cr3=deny')+1 and i+len(actual)<lines.index('STYLEOS S823 PASS'),'S835 proof placement')
    return report

def run(folder):
    meta=json.loads((folder/'results.json').read_text());need(meta.get('native_quarantine_835') is True,'S835 profile')
    parent=prior(folder);modes={}
    for mode,row in parent['modes'].items():
        if row.get('status')=='SKIPPED':modes[mode]=row;continue
        p=folder/mode;raw=(p/'serial-gated.log').read_bytes();trace=json.loads((p/'qmp-834.json').read_text());modes[mode]=verify_native(raw,33,mode,trace)
        print(mode,modes[mode]['cover'],modes[mode]['gap_pages'],flush=True)
    return dict(stage=835,status='PASS',modes=modes,allocator_integrated=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False,foundation_complete=False)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=run(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n')
