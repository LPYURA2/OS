"""Narrow Q35/PIIX3 observation; never a kernel I/O ownership grant."""
import re
from pathlib import Path
from block_fixture_856_verify import DEVICE, RAW, FILE, need
from overlay_fixture_856 import TOP, TOPFILE, verify_attachment

NAMES={RAW, FILE, TOP, TOPFILE}

def scope_fixture(o, base, top):
    """Allow unrelated boot/pflash nodes, refuse any alias into fixture graph."""
    g=o['graph']; vertices={v['id']:v for v in g['nodes']}
    need(len(vertices)==len(g['nodes']),'duplicate graph id')
    ids={i for i,v in vertices.items() if v.get('name') in NAMES|{DEVICE}}
    need(len(ids)==5,'fixture graph vertices')
    edges=[]
    for e in g['edges']:
        need(e['parent'] in vertices and e['child'] in vertices,'dangling graph edge')
        if e['parent'] in ids or e['child'] in ids:
            need(e['parent'] in ids and e['child'] in ids,'external fixture alias')
            edges.append(e)
    paths={str(Path(base).resolve()),str(Path(top).resolve())}
    nodes=[]
    for n in o['nodes']:
        if n.get('node-name') in NAMES: nodes.append(n)
        else: need(n.get('file') not in paths,'external file alias')
    blocks=[]
    for b in o['blocks']:
        i=b.get('inserted') or {}
        if b.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE):blocks.append(b)
        else:need(i.get('node-name') not in NAMES and i.get('file') not in paths,'external backend alias')
    # The full qtree and PCI inventory are preserved, not projected.
    projected=dict(o,nodes=nodes,blocks=blocks,graph={'nodes':[vertices[i] for i in ids],'edges':edges})
    result=verify_attachment(projected,base,top)
    result['scope']='same-boot-observation-only'
    return result

def verify_legacy_io(mtree):
    """Verify rendered port ranges, not exclusive ownership of generic regions."""
    need(isinstance(mtree,str),'mtree type')
    parts=re.split(r'(?m)^FlatView #\d+\r?$',mtree)
    io=[p for p in parts if re.search(r'(?m)^ AS "I/O", root: io\r?$',p)]
    need(len(io)==1,'unique IO flatview')
    ranges=[]
    for line in io[0].splitlines():
        if not line.strip() or line.startswith((' AS ',' Root memory region:')):continue
        m=re.fullmatch(r'\s+([0-9a-f]{16})-([0-9a-f]{16}) \(prio (-?\d+), i/o\): (.+)',line)
        need(m is not None,'unknown IO row')
        lo,hi=int(m[1],16),int(m[2],16)
        need(0<=lo<=hi<=65535,'IO extent')
        if ranges:need(ranges[-1][1]<lo,'overlap/ordering')
        ranges.append((lo,hi,int(m[3]),m[4]))
    for lo,hi in ((0x170,0x177),(0x376,0x376)):
        matches=[r for r in ranges if r[0]<=hi and r[1]>=lo]
        need(len(matches)==1,'legacy IDE coverage')
        a,b,prio,desc=matches[0]
        need((a,b,prio)==(lo,hi,0) and desc.startswith('ide owner:{obj type=memory-region-portio-list} '),'legacy IDE map mismatch')
    return {'secondary_legacy_ports_observed':True,'exclusive_owner_proven':False,'ata_admitted':False}
