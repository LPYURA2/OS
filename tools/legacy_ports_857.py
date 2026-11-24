"""Differential evidence for the fixed Q35/PIIX3 diagnostic, never a hardware grant."""
import json,re
from pathlib import Path
from block_fixture_856_verify import controller_identity,need,tree_device
PORTS=((0x170,0x177),(0x1f0,0x1f7),(0x376,0x376),(0x3f6,0x3f6))
DESC='ide owner:{obj type=memory-region-portio-list} parent:{obj path=/machine/unattached}'
def flat_io(s):
    views=[p for p in re.split(r'(?m)^FlatView #\d+\r?$',s) if re.search(r'(?m)^ AS "I/O", root: io\r?$',p)]
    need(len(views)==1,'unique I/O view');rows=[]
    for line in views[0].splitlines():
        if not line.strip() or line.startswith((' AS ',' Root memory region:')):continue
        m=re.fullmatch(r'\s+([0-9a-f]{16})-([0-9a-f]{16}) \(prio (-?\d+), i/o\): (.+)',line)
        need(m is not None,'I/O row');a,b,p=int(m[1],16),int(m[2],16),int(m[3]);need(0<=a<=b<=65535 and (not rows or rows[-1][1]<a),'I/O ordering');rows.append((a,b,p,m[4]))
    return rows

def identities(pci):
    need(isinstance(pci,list) and len(pci)==1 and pci[0]['bus']==0,'bus scope')
    devs=pci[0]['devices'];out={}
    for d in devs:
        b=d['slot']*8+d['function'];need(0<=b<256 and b not in out,'BDF')
        out[b]=(d['id']['vendor'],d['id']['device'],d['class_info']['class'],d.get('qdev_id'))
    return out

def check_map(mtree,present):
    rows=flat_io(mtree)
    for a,b in PORTS:
        hits=[r for r in rows if r[0]<=b and r[1]>=a]
        need(hits==([(a,b,0,DESC)] if present else [(0xf1,0x4cf,0,'io @00000000000000f1 parent:{obj path=/machine/unattached}')]),'legacy range mismatch')

def verify_pair(default,added):
    for x in (default,added):need(x['status'].get('running') is False,'not paused')
    cmd=added['command'];need(cmd==default['command']+['-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5'],'configuration delta')
    before,after=identities(default['pci']),identities(added['pci'])
    need(40 not in before and set(after)==set(before)|{40},'PCI delta')
    need(all(before[k]==after[k] for k in before),'other PCI change')
    need(after[40]==(0x8086,0x7010,0x0101,'styleos-ide'),'PIIX3 identity')
    need(sum(x[2]==0x0101 for x in after.values())==1,'unique IDE')
    need('dev: piix3-ide, id "styleos-ide"' not in default['qtree'] and added['qtree'].count('dev: piix3-ide, id "styleos-ide"')==1,'qtree delta')
    check_map(default['mtree'],False);check_map(added['mtree'],True)
    return {'four_ranges_appear_with_piix3':True,'scope':'paired paused Q35 fixture only','exclusive_owner_proven':False,'ata_admitted':False}

def verify_ready(ready):
    need(ready['status'].get('running') is False,'not paused')
    ids=identities(ready['pci']);need(ids.get(40)==(0x8086,0x7010,0x0101,'styleos-ide') and sum(v[2]==0x0101 for v in ids.values())==1,'unique IDE')
    tree_device(ready['qtree']);check_map(ready['mtree'],True)
    return {'four_ranges_observed':True,'exclusive_owner_proven':False,'ata_admitted':False}

def load(p,prefix=''):
    return {k:json.loads((p/(prefix+k+'.json')).read_text()) for k in ('status','pci','mtree','qtree')}

def pair_dir(p):
    x=load(p);x['command']=json.loads((p/'command.json').read_text());return x
