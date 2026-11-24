import json,sys
from pathlib import Path
from busmaster_857_verify import verify_transition,KEYS,POLICY
from storage_topology_856_verify import verify
p=Path(sys.argv[1]);raw=(p/'serial.log').read_bytes();result=json.loads((p/'result.json').read_text());root=result['root'];pci=json.loads((p/'ready-pci.json').read_text());t=verify(raw,root,pci)
vals=(root,40,0x02800007,0x02800003,0x60e1,0,0,0,0)
def append(v):return raw+b'\nSTYLEOS S857 BUSMASTER '+b' '.join(f'{k}=0x{x:016x}'.encode() for k,x in zip(KEYS,v))+b'\n'+POLICY+b'\n'
valid=append(vals);assert verify_transition(valid,root,t)['pci_bus_master_cleared']
count=0
for i,v in enumerate(vals):
    for bit in range(64):
        a=list(vals);a[i]^=1<<bit
        try:verify_transition(append(a),root,t)
        except ValueError:count+=1;continue
        raise AssertionError('mutation accepted')
for bad in (raw,valid+POLICY+b'\n',valid.replace(b'ata-access=no dma-admission=no',b'ata-access=yes dma-admission=no'),valid+b'STYLEOS S850 FAIL fake\n',valid.replace(b'primary-status=0x0000000000000000',b'primary-status=0x0000000000000002',1)):
    try:verify_transition(bad,root,t)
    except ValueError:count+=1;continue
    raise AssertionError('bad evidence accepted')
print('Actual prior capture + synthetic transition: 1 positive /',count,'refusals PASS; real transition NOT RUN')
