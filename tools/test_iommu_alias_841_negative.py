#!/usr/bin/env python3
"""Mutate genuine S841 captures after their full parent and census replay.
These are host refusals, not additional hardware observations."""
import argparse,json,re
from pathlib import Path
from iommu_alias_841_verify import run,replay,POLICY
from ecam_quarantine_827_verify import need
def test(folder,report_path):
    original=run(folder)
    need(original==json.loads(report_path.read_text()),'original S841 replay differs')
    total=0; modes={}
    for mode,row in original['modes'].items():
        if row.get('status')=='SKIPPED':continue
        p=folder/mode;raw=(p/'serial.log').read_bytes();efi=(p/'final-map.bin').read_bytes()
        lines=raw.decode('ascii').splitlines()
        obs=[x for x in lines if x.startswith('STYLEOS S841 ')]
        start=lines.index(obs[0]);end=start+len(obs)
        need(lines[start:end]==obs,'fixture contiguous')
        def packet(o):return ('\n'.join(lines[:start]+o+lines[end:])+'\n').encode('ascii')
        need(replay(packet(obs),efi,mode)==row,'unaltered control')
        def index(tag):return next(i for i,x in enumerate(obs) if x.startswith('STYLEOS S841 '+tag+' '))
        path=index('PATH');first=index('ROW');last=index('TOTAL')
        mutations=[]
        def change(i,key,xor):
            bad=obs.copy()
            bad[i]=re.sub(r' '+key+r'=0x([0-9a-f]{16})',lambda m:f' {key}=0x{(int(m[1],16)^xor):016x}',bad[i],count=1)
            need(bad!=obs,'mutation changed bytes');mutations.append(bad)
        for key,xor in [('pa',4096),('prefix',4096),('level',1),('admitted',1)]:change(path,key,xor)
        change(first,'slot',16)
        # Slot zero in the root is already bound to the parent live proof.
        change(first,'e',4096)
        change(last,'paths',1);change(last,'denied',1)
        mutations += [obs[:first]+obs[first+1:],obs[:first]+[obs[first+1],obs[first]]+obs[first+2:],obs[:first]+[obs[first]]+obs[first:],obs[:-1],obs+[POLICY],obs[:-1]+[POLICY.replace('production-cr3=deny','production-cr3=allow')]]
        # Append an invented path before totals, even if otherwise syntactically valid.
        mutations.append(obs[:last]+[obs[path]]+obs[last:])
        for i,bad in enumerate(mutations):
            try:replay(packet(bad),efi,mode)
            except (ValueError,KeyError,IndexError,StopIteration,AssertionError):continue
            raise AssertionError(f'{mode}: forged S841 record accepted {i}')
        modes[mode]=len(mutations);total+=len(mutations)
    need(total>0,'no negative cases')
    print(json.dumps(dict(stage=841,host_refusals=total,modes=modes,guest_evidence=False)))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);p.add_argument('--report',required=True,type=Path)
    a=p.parse_args();test(a.input,a.report)
