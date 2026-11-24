"""Exact terminal PIIX3 command transition; no ATA or DMA admission."""
import re
from bmdma_857_verify import verify_record
POLICY=b'STYLEOS S857 BUSMASTER-POLICY pci-master-cleared=yes ata-access=no dma-admission=no production-cr3=deny'
KEYS=('root','bdf','before','after','bar4','primary-command','primary-status','secondary-command','secondary-status')
def verify_transition(raw,root,topology):
    if type(root) is not int or not 0<root<2**64 or root&4095:raise ValueError('root')
    if any(b' FAIL ' in x for x in raw.splitlines()):raise ValueError('guest failure')
    rows=[x for x in raw.splitlines() if x.startswith(b'STYLEOS S857 ')]
    if len(rows)!=4 or rows[-1]!=POLICY:raise ValueError('transition policy/order')
    observation=verify_record(b'\n'.join(rows[:2]),root,topology)
    if any(observation[k] for k in ('primary_start','primary_active','secondary_start','secondary_active')) or not observation['bus_master_enabled']:raise ValueError('prior state')
    if not re.search(rb'primary-command=0x0{16} primary-status=0x0{16} secondary-command=0x0{16} secondary-status=0x0{16} samples=0x0{15}2$',rows[0]):raise ValueError('prior BMIDE bytes')
    m=re.fullmatch(b'STYLEOS S857 BUSMASTER '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in KEYS),rows[2])
    if not m:raise ValueError('transition schema')
    values=tuple(int(x,16) for x in m.groups())
    if values!=(root,40,0x02800007,0x02800003,0x60e1,0,0,0,0):raise ValueError('transition values')
    if topology['ide_config'][40][0]!=values[2]:raise ValueError('prior PCI config')
    return {'pci_bus_master_cleared':True,'before':7,'after':3,'dma_admission':False,'ata_access':False}
