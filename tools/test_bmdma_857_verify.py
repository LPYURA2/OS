import json,sys
from pathlib import Path
from copy import deepcopy
from bmdma_857_verify import verify_bar,verify_record,POLICY
from storage_topology_856_verify import verify as topology
p=Path(sys.argv[1]);pci=json.loads((p/'ready-pci.json').read_text());mt=json.loads((p/'ready-mtree.json').read_text());raw=(p/'serial.log').read_bytes();root=json.loads((p/'result.json').read_text())['root'];t=topology(raw,root,pci)
raw=b"\n".join(l for l in raw.splitlines() if not l.startswith(b"STYLEOS S857 "))+b"\n"
assert verify_bar(pci,mt)
keys=('root','bdf','bar4','pci-command','primary-command','primary-status','secondary-command','secondary-status','samples')
vals=(root,40,0x60e1,t["ide_config"][40][0]&65535,0,0,0,0,2)
record=lambda v:b'STYLEOS S857 BMIDE '+b' '.join(f'{k}=0x{x:016x}'.encode() for k,x in zip(keys,v))+b'\n'+POLICY+b'\n'
assert verify_record(raw+record(vals),root,t)['admission'] is False
v=list(vals);v[6]=1;v[7]=1;assert verify_record(raw+record(v),root,t)['secondary_active']
count=0
for index,bad in [(0,0),(1,41),(2,0x60e5),(3,vals[3]^4),(4,256),(5,255),(6,255),(7,256),(8,1)]:
    v=list(vals);v[index]=bad
    try:verify_record(raw+record(v),root,t)
    except ValueError:count+=1;continue
    raise AssertionError('bad record accepted')
for b in (raw,raw+record(vals)*2,raw+record(vals).replace(b'admission=no',b'admission=yes')):
    try:verify_record(b,root,t)
    except ValueError:count+=1;continue
    raise AssertionError('bad policy accepted')
for key,value in [('bar',3),('size',8),('address',0x60c0),('type','memory')]:
    x=deepcopy(pci);d=next(d for b in x for d in b['devices'] if d.get('qdev_id')=='styleos-ide');d['regions'][0][key]=value
    try:verify_bar(x,mt)
    except ValueError:count+=1;continue
    raise AssertionError('bad BAR accepted')
for a,b in [('60e0-00000000000060e3','60e0-00000000000060e2'),('piix-bmdma owner:{dev id=styleos-ide}','piix-bmdma owner:{dev id=foreign}'),('60eb (prio 0','60eb (prio 1'),('60ef (prio 0, i/o): bmdma','60ef (prio 0, i/o): foreign')]:
    assert a in mt
    try:verify_bar(pci,mt.replace(a,b))
    except ValueError:count+=1;continue
    raise AssertionError('bad map accepted')
print('Actual BAR4 QMP replay + synthetic idle/active records: 3 positive /',count,'refusals PASS; no DMA admission')
