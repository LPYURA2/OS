"""Exact fixture BMIDE port ownership and guest readback, no admission."""
import re
from block_fixture_856_verify import controller_identity, need
POLICY=b'STYLEOS S857 POLICY bmdma-read=yes bmdma-write=no ata-access=no dma-denied=no admission=no production-cr3=deny'
def verify_bar(pci,mtree,expected_base=0x60e0):
    need(expected_base in (0x60e0,0x6060),'unsupported observed BAR4 window')
    matches=[d for b in pci if b['bus']==0 for d in b['devices'] if d.get('qdev_id')=='styleos-ide']
    need(len(matches)==1 and controller_identity(matches[0]),'controller')
    r=matches[0]['regions'];need(len(r)==1 and r[0].get('bar')==4 and r[0].get('size')==16 and r[0].get('type')=='io' and r[0].get('address')==expected_base,'BAR4 extent')
    io=[p for p in re.split(r'(?m)^FlatView #\d+\r?$',mtree) if re.search(r'(?m)^ AS "I/O", root: io\r?$',p)]
    need(len(io)==1,'IO view');rows=[]
    for line in io[0].splitlines():
        if not line.strip() or line.startswith((' AS ',' Root memory region:')):continue
        m=re.fullmatch(r'\s+([0-9a-f]{16})-([0-9a-f]{16}) \(prio (-?\d+), i/o\): (.+)',line)
        need(m is not None,'IO row');lo,hi=int(m[1],16),int(m[2],16)
        if lo<=expected_base+15 and hi>=expected_base:rows.append((lo,hi,int(m[3]),m[4]))
    expected=[(expected_base+4*i,expected_base+4*i+3,0,('piix-bmdma' if i%2==0 else 'bmdma')+' owner:{dev id=styleos-ide}') for i in range(4)]
    need(rows==expected,'BMIDE owner or extent');return True

def verify_record(raw,root,t):
    rows=[x for x in raw.splitlines() if x.startswith(b'STYLEOS S857 ')]
    need(len(rows)==2 and rows[1]==POLICY,'BMIDE policy')
    keys=('root','bdf','bar4','pci-command','primary-command','primary-status','secondary-command','secondary-status','samples')
    m=re.fullmatch(b'STYLEOS S857 BMIDE '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),rows[0]);need(m is not None,'record')
    a,b,c,d,e,f,g,h,i=(int(x,16) for x in m.groups());need(a==root and b==40 and c==0x60e1 and i==2,'binding')
    v=t['ide_config'].get(40);need(v is not None and d==(v[0]&65535) and c==v[7] and d&1,'PCI readback')
    need(all(0<=x<256 for x in (e,f,g,h)) and all(x!=255 for x in (e,f,g,h)),'status byte')
    return {'primary_start':bool(e&1),'primary_active':bool(f&1),'secondary_start':bool(g&1),'secondary_active':bool(h&1),'bus_master_enabled':bool(d&4),'dma_denied':False,'admission':False}
