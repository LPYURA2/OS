"""Synthetic ownership and composition tests only; no QEMU assertion."""
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path[:0]=[str(root/'tests'),str(root/'tools')]
from test_ownership_output import fixture
from test import serial_ownership_ok,ownership_report
from iommu_guest_837_verify import expected,verify_composition,merge
count=0
raw,site=fixture('x86_64');prefix=raw.removesuffix(b'STYLEOS KERNEL HALT\n')
assert serial_ownership_ok(prefix,'x86_64',site,terminal=False) and not serial_ownership_ok(prefix,'x86_64',site);count+=1
assert not serial_ownership_ok(raw,'x86_64',site,terminal=False);count+=1
owner=ownership_report(prefix,'x86_64')
res=[[x['start'],x['end']] for x in owner['protected']];rev=[[x['start'],x['end']] for x in owner['review']]
q=dict(reserved=merge(res+[[0xfed90000,0xfed91000]]),candidates=rev,allocator_integrated=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)
rows,want=expected(owner,q);block=('\n'.join(rows)+'\n').encode();anchor=b'STYLEOS S835 POLICY fixture-only=yes detached-map=yes ram-grants=no mmio=not-read dma-grants=no production-cr3=deny\n';sample=prefix+anchor+block+b'STYLEOS S823 FIRMWARE fixture\nSTYLEOS S823 PASS\n'
assert verify_composition(sample,owner,q)==want;count+=1
for bad in (sample.replace(block,b''),sample.replace(block,block+block),sample.replace(b'granted=0x0000000000000000\nSTYLEOS S837',b'granted=0x0000000000000001\nSTYLEOS S837'),sample.replace(b'STYLEOS S837 PROTECTED',b'STYLEOS S837 REVIEW',1),sample.replace(anchor,b''),sample.replace(block,b'noise\n'+block)):
 try:verify_composition(bad,owner,q)
 except (ValueError,KeyError):count+=1
 else:raise AssertionError('accepted changed proof')
for k in ('allocator_integrated','ownership','mmio_authorized','dma_authorized','production_cr3_authorized'):
 try:expected(owner,dict(q,**{k:True}))
 except ValueError:count+=1
 else:raise AssertionError('grant accepted')
for r in ([[0,4096]],rev[:-1]):
 try:expected(owner,dict(q,candidates=r))
 except ValueError:count+=1
 else:raise AssertionError('missing candidate accepted')
print(f'{count}/{count} synthetic verifier checks PASS')
