"""Paused fixture proof only: writable disposable top, immutable RAW backing."""
from pathlib import Path
from block_fixture_856_verify import controller_identity, tree_device, DEVICE, RAW, FILE, need
from sector_evidence_856 import N, SIZE
TOP='s856-top'; TOPFILE='s856-top-file'; LENGTH=N*SIZE
ZERO=('wr_bytes','wr_operations','flush_operations','unmap_bytes','unmap_operations')

def verify_map(rows):
    need(isinstance(rows,list) and rows,'map schema')
    pos=0
    for row in rows:
        need(isinstance(row,dict),'map entry')
        need(all(type(row.get(k)) is int for k in ('start','length','depth')),'map numbers')
        need(row['start']==pos and row['length']>0 and row['depth']==1 and row.get('data') is True,'map not pristine backing')
        pos+=row['length'];need(pos<=LENGTH,'map overflow')
    need(pos==LENGTH,'map coverage')

def verify_stats(rows):
    need(isinstance(rows,list),'stats schema')
    matches=[r for r in rows if isinstance(r,dict) and r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE)]
    need(len(matches)==1 and matches[0].get('node-name')==TOP,'frontend stats identity')
    s=matches[0].get('stats');need(isinstance(s,dict),'stats object')
    for k in ZERO:need(type(s.get(k)) is int and s[k]==0,'nonzero/missing '+k)
    for k in ('failed_wr_operations','invalid_wr_operations','failed_unmap_operations','invalid_unmap_operations','failed_flush_operations','invalid_flush_operations'):
        if k in s:need(type(s[k]) is int and s[k]==0,'failed/invalid operation '+k)

def verify_attachment(o,base,top):
    need(o['status'].get('running') is False,'guest not paused')
    need(o['unit']==0 and type(o['unit']) is int,'unit')
    devices=[d for b in o['pci'] if b['bus']==0 for d in b['devices'] if d.get('qdev_id')=='styleos-ide']
    need(len(devices)==1 and controller_identity(devices[0]),'controller')
    tree_device(o['qtree'])
    wanted={FILE:('file',True,str(Path(base).resolve())),RAW:('raw',True,str(Path(base).resolve())),TOPFILE:('file',False,str(Path(top).resolve())),TOP:('qcow2',False,str(Path(top).resolve()))}
    nodes=o['nodes'];need(isinstance(nodes,list) and len(nodes)==4,'unexpected named nodes')
    for name,(driver,ro,path) in wanted.items():
        m=[n for n in nodes if n.get('node-name')==name];need(len(m)==1,'node identity')
        n=m[0];need(n.get('drv')==driver and n.get('ro') is ro and n.get('file')==path,'node mode/path')
        if name==TOP:need(n.get('image',{}).get('virtual-size')==LENGTH,'top size')
    g=o['graph'];verts={v['id']:v for v in g['nodes']};need(len(verts)==len(g['nodes'])==5,'graph vertices')
    ids={}
    for name in wanted:
        m=[i for i,v in verts.items() if v.get('name')==name and v.get('type')=='block-driver'];need(len(m)==1,'graph driver');ids[name]=m[0]
    backend=[i for i,v in verts.items() if v.get('type')=='block-backend'];need(len(backend)==1,'backend')
    expected={(backend[0],ids[TOP],'root'),(ids[TOP],ids[TOPFILE],'file'),(ids[TOP],ids[RAW],'backing'),(ids[RAW],ids[FILE],'file')}
    edges=g['edges'];need(len(edges)==4 and {(e['parent'],e['child'],e['name']) for e in edges}==expected,'graph relation')
    for e in edges:
        if e['child'] in (ids[RAW],ids[FILE]):need(isinstance(e.get('perm'),list) and set(e['perm'])=={'consistent-read'},'backing permission')
    blocks=o['blocks'];need(len(blocks)==1,'unexpected backend')
    b=blocks[0];i=b['inserted'];need(b.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE) and b.get('removable') is False,'attachment')
    need(i.get('node-name')==TOP and i.get('ro') is False and i.get('drv')=='qcow2' and i.get('image',{}).get('virtual-size')==LENGTH,'top attachment')
    verify_stats(o['stats'])
    return {'paused':True,'base_read_only':True,'top_writable':True,'frontend_mutations':0,'ata_admitted':False}
