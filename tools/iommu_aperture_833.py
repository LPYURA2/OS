"""Correlate initial QEMU flat map and final same-process guest map; deny-only."""
import argparse,hashlib,json
from pathlib import Path
from irq_sources_830_verify import verify,BACKEND,need
from replay_dma_association_831 import replay as prior
from test_acpi_windows_825 import reconcile
from uefi_map_826 import audit
from qmp_aperture_833 import extract,aperture

def overlaps(a,b):return a[0]<b[1] and b[0]<a[1]

def union(spans):
    out=[]
    for a,b in sorted(spans):
        need(0<=a<b<=2**64,'range extent')
        if out and a<=out[-1][1]:out[-1][1]=max(b,out[-1][1])
        else:out.append([a,b])
    return out

def correlate(raw,rc,mode,trace):
    ok,d=verify(raw,rc,mode);need(ok,'S830 UART replay')
    units=d['s828_inventory']['units'];spans=aperture(extract(trace),BACKEND[mode],units)
    m=d['s826_map_audit'];e=d['s827_exclusion'];w=reconcile(d)
    windows=[v[1:] for v in w['producer_windows'] if v[0]==0]+[[v['start'],v['end']] for v in w['allocated_windows'] if v['kind']==0]
    other=[w['ecam_interval']]+windows+[[v['address'],v['address']+4096] for v in d['s830_ioapics']]
    result=[]
    for a,b in spans:
        hit=audit(d['_s826_raw']['final-map.bin'],m['descriptor_size'],m['descriptor_version'],a,b)
        need(not hit['conflicts'] and not any(overlaps([a,b],r) for r in other+e['candidates']),'aperture conflicts with observed resources')
        result.append(dict(interval=[a,b],bytes=b-a,uefi_status=hit['status'],covered=hit['covered'],gaps=hit['gaps'],conflicts=hit['conflicts']))
    denied=union(e['reserved']+spans);need(not any(overlaps(a,b) for a in denied for b in e['candidates']),'deny map conflict')
    return dict(mode=mode,initial_qemu_model_apertures=result,detached_denied=denied,unchanged_candidates=e['candidates'],post_ebs_qemu_aperture_verified=False,physical_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)

def run(folder):
    parent=prior(folder);meta=json.loads((folder/'results.json').read_text());need(meta.get('qmp_833_capture') is True,'capture profile absent')
    modes={}
    for mode,row in parent['modes'].items():
        if row['status']=='SKIPPED':modes[mode]={'status':'SKIPPED'};continue
        path=folder/mode;raw=(path/'serial.log').read_bytes();trace=(path/'qmp-833.json').read_bytes();digest=hashlib.sha256(trace).hexdigest()
        need(digest==meta['modes'][mode]['qmp_833_sha256'],'QMP digest mismatch')
        r=correlate(raw,33,mode,json.loads(trace));r['qmp_sha256']=digest;r['uart_sha256']=hashlib.sha256(raw).hexdigest();modes[mode]=r
        print(mode,[(hex(v['interval'][0]),v['bytes'],v['uefi_status']) for v in r['initial_qemu_model_apertures']],flush=True)
    return dict(stage=833,status='PASS',modes=modes,initial_model_only=True,physical_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False,foundation_complete=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();r=run(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n')
