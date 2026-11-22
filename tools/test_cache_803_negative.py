#!/usr/bin/env python3
"""Synthetic/forged fixture regression. Never presented as hardware proof."""
from cache_803_verify import verify,KEYS,POLICY
from cache_800_verify import verify as verify800
from cache_801_verify import verify as verify801
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
arch=Path('/mnt/data/StyleOS-802-GitHub-Actions.zip')
# Portable fixture: values captured in 800/801 but combined artificially only for negative tests.
# In CI the original artifact is not expected to be mounted; build from fixed valid sample.
def record(kind, fields):
 return b'STYLEOS S803 '+kind.encode()+b' '+b' '.join(k.encode()+b'=0x'+format(fields[k],'016x').encode() for k in KEYS[kind])
CPU={'cr0':0x80010033,'cr3':0x07801000,'pat':0x0007040600070406,'mtrrcap':0x508,'mtrrdef':0xc06,'physbits':40,'variable-count':8}
PCI={'id':0x10431af4,'gop':0x80000000,'bar1raw':0x81000000,'bar4raw':0x0000000c,'bar5raw':0xc0}
MTRR=[{'index':i,'base':6,'mask':0} for i in range(8)]
# MTRR default 6 does not prove UC, so a synthesized UC matching entry is required.
MTRR[0]={'index':0,'base':0,'mask':0x800}
# Identity-mapped 2MiB leaf, WB PAT, MTRR UC.
TARGET=[]
for i,a in enumerate((0x80000000,0x81000000,0xc000000000)):
 TARGET.append({'index':i,'address':a,'mtrr-type':0,'matches':1,'enabled':1,'present':1,
    'translated':a,'page-size':2097152,'entry':a|0x83,'pat-index':0,'pat-type':6,'effective-sample-type':0})
lines=[b'STYLEOS x86_64 ExitBootServices OK',record('CPU',CPU),record('PCI',PCI)]
lines += [record('MTRR',r) for r in MTRR]
lines += [record('TARGET',r) for r in TARGET]
lines += [POLICY,b'STYLEOS S803 PASS']
valid=b'\n'.join(lines)+b'\n'
def wrong(data,where,needle,replacement):
 assert needle in data,where
 return data.replace(needle,replacement,1)
# This is deliberately a parsable synthetic payload, never a QEMU pass claim.
assert verify(valid,33)[0],verify(valid,33)
variants=[
 ('wrong-qemu-exit',valid,0),('duplicate-pass',valid+b'STYLEOS S803 PASS\n',33),
 ('missing-policy',valid.replace(POLICY,b''),33),
 ('fake-global-alias',valid.replace(b'global-alias-scan=unverified',b'global-alias-scan=verified'),33),
 ('fake-production-cr3',valid.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
 ('missing-handoff',valid.replace(b'STYLEOS x86_64 ExitBootServices OK',b'NO HANDOFF'),33),
 ('wrong-phys-translation',wrong(valid,'addr',b'translated=0x0000000080000000',b'translated=0x0000000080001000'),33),
 ('wrong-cache-type',wrong(valid,'cache',b'effective-sample-type=0x0000000000000000',b'effective-sample-type=0x0000000000000006'),33),
 ('wrong-pat-index',wrong(valid,'pat',b'pat-index=0x0000000000000000',b'pat-index=0x0000000000000001'),33),
 ('wrong-mtrr',wrong(valid,'mtrr',b'mtrr-type=0x0000000000000000',b'mtrr-type=0x0000000000000006'),33),
 ('wrong-count',wrong(valid,'count',b'variable-count=0x0000000000000008',b'variable-count=0x0000000000000007'),33),
 ('missing-target',valid.replace(record('TARGET',TARGET[2])+b'\n',b''),33),
 ('double-target',valid.replace(record('TARGET',TARGET[2])+b'\n',2*(record('TARGET',TARGET[2])+b'\n')),33),
 ('tampered-bar',wrong(valid,'bar',b'bar1raw=0x0000000081000000',b'bar1raw=0x0000000081001000'),33),
 ('wrong-page-size',wrong(valid,'size',b'page-size=0x0000000000200000',b'page-size=0x0000000000001000'),33),
 ('missing-mmio',valid.replace(b'STYLEOS S803 TARGET',b'STYLEOS S803 FRAGMENT',1),33),
]
for name,data,rc in variants:
 if verify(data,rc)[0]:raise AssertionError('unsafe acceptance '+name)
print('STYLEOS 803 negative UART cases PASS',len(variants))
