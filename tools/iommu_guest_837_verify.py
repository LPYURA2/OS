"""Draft independent same-boot composition verifier. Requires exact binary site proof."""
import re
from test import serial_ownership_ok,ownership_report
from iommu_quarantine_835_verify import verify_native
from post_ebs_aperture_834 import ungate
from acpi_ecam_826_verify import verify
from uefi_map_826 import parse
from ecam_quarantine_827_verify import merge,need
P=4096
POLICY='STYLEOS S837 POLICY detached=yes allocator-integrated=no'
PASS=b'STYLEOS OWNERSHIP PASS'

def subtract(ram,reserved):
    pieces=[]
    for a,b in ram:
        for lo,hi in reserved:
            if hi<=a:continue
            if lo>=b:break
            if a<lo:pieces.append([a,lo])
            a=max(a,hi)
            if a>=b:break
        if a<b:pieces.append([a,b])
    return merge(pieces)

def check_live_map(prefix,data,stride,version,owner):
    rows=parse(data,stride,version);need(len(rows)<=256,'map capacity')
    ram=[];reserved=[]
    for r in rows:
        attr=r['attributes'];(ram if r['type']==7 and attr&8 and not attr&~0x401f else reserved).append([r['start'],r['end_exclusive']])
    actual={}
    for s in prefix.decode('ascii').splitlines():
        m=re.fullmatch(r'STYLEOS MEMMAP (IMAGE|STACK|HANDOFF|SOURCE|RAM|RESERVED|CANDIDATE) 0x([0-9a-f]{16}) 0x([0-9a-f]{16})',s)
        if m:actual.setdefault(m[1],[]).append([int(m[2],16),int(m[3],16)])
    for k in ('IMAGE','HANDOFF','SOURCE'):
        need(len(actual[k])==1,'live range count');a,b=actual[k][0];reserved.append([a&~4095,(b+4095)&~4095])
    reserved=merge(reserved)
    need(actual['RAM']==ram and actual['RESERVED']==reserved and actual['CANDIDATE']==subtract(ram,reserved),'live map differs from final EFI bytes')
    src=owner['source_data'];need(actual['SOURCE']==[[src['pa'],src['pa']+src['bytes']]] and src['bytes']==len(data) and src['stride']==stride and src['version']==version,'map provenance')

def expected(owner,q):
    need(owner['totals']['granted']==0 and q['allocator_integrated'] is False and q['ownership'] is False and q['mmio_authorized'] is False and q['dma_authorized'] is False and q['production_cr3_authorized'] is False,'authority')
    protect=merge([[r['start'],r['end']] for r in owner['protected']]+q['reserved'])
    review=[[r['start'],r['end']] for r in owner['review']]
    need(all(any(a<=lo and hi<=b for a,b in q['candidates']) for lo,hi in review),'review outside candidates')
    need(all(b<=lo or hi<=a for a,b in protect for lo,hi in review),'protection overlap')
    hx=lambda x:f'0x{x:016x}'
    lines=[f'STYLEOS S837 {label} start={hx(a)} end={hx(b)}' for label,ranges in [('PROTECTED',protect),('REVIEW',review)] for a,b in ranges]
    lines.append(f"STYLEOS S837 TOTAL protected={hx(sum(b-a for a,b in protect)//P)} review={hx(sum(b-a for a,b in review)//P)} granted={hx(0)}")
    return lines+[POLICY],{'protected':protect,'review':review,'granted':0,'allocator_integrated':False}

def verify_composition(raw,owner,q):
    lines=raw.decode('ascii').replace('\r','').splitlines();want,report=expected(owner,q)
    got=[s for s in lines if s.startswith('STYLEOS S837 ')]
    need(got==want,'composition report mismatch');i=lines.index(got[0]);need(lines[i:i+len(got)]==got,'composition order')
    anchor='STYLEOS S835 POLICY fixture-only=yes detached-map=yes ram-grants=no mmio=not-read dma-grants=no production-cr3=deny'
    need(i==lines.index(anchor)+1 and lines.index('STYLEOS OWNERSHIP PASS')<i and i+len(got)<lines.index('STYLEOS S823 PASS'),'composition placement')
    return report

def replay(raw,rc,mode,trace,site):
    old=ungate(raw);q=verify_native(raw,rc,mode,trace)
    lines=old.splitlines(keepends=True);matches=[i for i,v in enumerate(lines) if v.rstrip(b'\r\n')==PASS]
    need(len(matches)==1,'ownership proof count');prefix=b''.join(lines[:matches[0]+1])
    need(serial_ownership_ok(prefix,'x86_64',site,terminal=False),'ownership proof')
    owner=ownership_report(prefix,'x86_64');ok,d=verify(old,rc,mode);need(ok,'parent map')
    m=d['s826_map_audit'];check_live_map(prefix,d['_s826_raw']['final-map.bin'],m['descriptor_size'],m['descriptor_version'],owner)
    return verify_composition(old,owner,q)


def run(folder):
    import hashlib,json
    from pathlib import Path
    from test import verified_ownership_site
    from iommu_quarantine_835_verify import run as prior
    folder=Path(folder);meta=json.loads((folder/'results.json').read_text())
    need(meta.get('guest_plan_837') is True,'S837 profile missing')
    site=verified_ownership_site(folder/'build','x86_64',guest_plan_837=True)
    need(site==meta['ownership_site_837'] and site['binary_sha256']==meta['efi_sha256'],'S837 image binding')
    parent=prior(folder);modes={}
    for mode,row in parent['modes'].items():
        if row.get('status')=='SKIPPED':modes[mode]=row;continue
        p=folder/mode;raw=(p/'serial-gated.log').read_bytes();trace=json.loads((p/'qmp-834.json').read_text());m=meta['modes'][mode]
        need(m['qemu_exit_code']==33,'S837 exit status')
        r=replay(raw,33,mode,trace,site)
        need(r==m['composition_837'],'S837 recorded composition mismatch')
        modes[mode]=r
    return dict(stage=837,status='PASS',modes=modes,allocator_integrated=False,foundation_complete=False)

if __name__=='__main__':
    import argparse,json
    from pathlib import Path
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=run(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n');print('S837 replay PASS')
