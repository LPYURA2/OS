#!/usr/bin/env python3
"""Stage 798 hardware provenance-bound conservative aperture report."""
import argparse,hashlib,json
from pathlib import Path
from pci_mmio_extent_798 import inspect

def read_evidence(path,stage):
    raw=(path/'serial.log').read_bytes()
    j=json.loads((path/'results.json').read_text())
    if j.get('stage')!=stage or j.get('uart_sha256')!=hashlib.sha256(raw).hexdigest():
        raise ValueError('hardware provenance/sha mismatch')
    return raw,j

def main():
    p=argparse.ArgumentParser();
    for n in ('aperture','default','bridge','forwarding','output'):
        p.add_argument('--'+n,required=True,type=Path)
    a=p.parse_args()
    ap,ar=read_evidence(a.aperture,795)
    tops={};results={}
    for name in ('default','bridge'):
        tops[name],results[name]=read_evidence(getattr(a,name),796)
    fr=json.loads((a.forwarding/'results.json').read_text())
    out=inspect(ap,ar,tops,results,fr)
    if (not out['verified_apertures'] or not out['topologies']['bridge']['resources_without_full_aperture_attestation'] or
        out['production_cr3_authorized'] or out['mmio_mapping_authorized'] or out['all_mmio_apertures_verified']):
        raise RuntimeError('unsafe MMIO audit output')
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/'results.json').write_text(json.dumps(out,indent=2,sort_keys=True)+'\n')
    print('STYLEOS S798 SAME-CI HARDWARE-BASED EXTENT AUDIT PASS: VGA sized, other MMIO denied')
if __name__=='__main__':main()
