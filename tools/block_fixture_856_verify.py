"""Strict QMP/HMP fixture correlation; not an ATA or DMA grant."""
from pathlib import Path
import re
from sector_evidence_856 import fixture, N, SIZE

CONTROLLER='styleos-ide'
DEVICE='s856-disk'
RAW='s856-raw'
FILE='s856-file'

def need(condition, message):
    if not condition: raise ValueError(message)

def tree_device(tree):
    """Find direct parent bus/controller in HMP info qtree indentation tree."""
    need(isinstance(tree,str), 'qtree type')
    stack=[]; found=[]; controllers=[]; siblings=[]
    for line in tree.splitlines():
        if not line.strip(): continue
        need('\t' not in line, 'qtree tabs')
        indent=len(line)-len(line.lstrip(' ')); body=line.strip()
        m=re.fullmatch(r'dev: ([^,]+), id "([^"]*)"',body)
        b=re.fullmatch(r'bus: (\S+)',body)
        if not (m or b): continue
        while stack and stack[-1][0]>=indent: stack.pop()
        if m:
            node=(indent,'dev',m[1],m[2])
            if m[2]==CONTROLLER: controllers.append(node)
            if m[2]==DEVICE: found.append((node,tuple(stack)))
            if len(stack)>=2 and stack[-1][1:] == ('bus',CONTROLLER+'.1','') and stack[-2][1:] == ('dev','piix3-ide',CONTROLLER): siblings.append(node)
        else: node=(indent,'bus',b[1],'')
        stack.append(node)
    need(len(controllers)==1 and controllers[0][1:]==('dev','piix3-ide',CONTROLLER),'controller qtree')
    need(len(found)==1,'device qtree')
    node,parents=found[0]
    need(node[2]=='ide-hd' and len(parents)>=2 and parents[-1][1:]==('bus',CONTROLLER+'.1','') and parents[-2]==controllers[0],'wrong IDE channel')
    need(siblings==[node],'extra secondary IDE device')

def verify_fixture(*, blocks, nodes, qtree, unit, pci, graph, path, original, current, seed):
    # This verifier deliberately refuses unrecognized QMP schema or topology.
    need(type(unit) is int and unit==0,'not secondary master')
    need(isinstance(blocks,list) and isinstance(nodes,list) and isinstance(pci,list),'QMP list schema')
    need(original==fixture(seed) and current==original,'fixture content')
    path=str(Path(path).resolve())
    need(all(isinstance(x,dict) for x in blocks+nodes+pci),'QMP item schema')
    devices=[d for b in pci if b.get('bus')==0 for d in b.get('devices',[])]
    matches=[d for d in devices if d.get('qdev_id')==CONTROLLER]
    need(len(matches)==1,'controller QMP identity')
    d=matches[0]
    need(d.get('slot')==5 and d.get('function')==0 and d.get('id')=={'vendor':0x8086,'device':0x7010} and d.get('class_info',{}).get('class')==0x0101,'controller PCI address/class')
    tree_device(qtree)
    named={}
    for n in nodes:
        name=n.get('node-name')
        if name in (RAW,FILE):
            need(name not in named,'duplicate node');named[name]=n
    need(set(named)=={RAW,FILE},'missing named node')
    for name,driver in ((RAW,'raw'),(FILE,'file')):
        n=named[name]
        need(n.get('drv')==driver and n.get('ro') is True and n.get('file')==path,'node driver/RO/path')
        need(n.get('backing_file_depth')==0 and not n.get('backing_file') and not n.get('image',{}).get('backing-filename'),'backing forbidden')
        need(n.get('image',{}).get('virtual-size')==N*SIZE,'node size')
    need(isinstance(graph,dict) and isinstance(graph.get('nodes'),list) and isinstance(graph.get('edges'),list),'graph schema')
    vertices={}
    for n in graph['nodes']:
        need(isinstance(n,dict) and type(n.get('id')) is int and n['id'] not in vertices,'graph node id')
        vertices[n['id']]=n
    def vertex(name):
        ids=[i for i,n in vertices.items() if n.get('type')=='block-driver' and n.get('name')==name]
        need(len(ids)==1,'graph named driver'); return ids[0]
    ri,fi=vertex(RAW),vertex(FILE)
    edges=graph['edges']
    for e in edges:
        need(isinstance(e,dict) and e.get('parent') in vertices and e.get('child') in vertices and isinstance(e.get('perm'),list),'graph edge schema')
    relevant=[e for e in edges if e['parent'] in (ri,fi) or e['child'] in (ri,fi)]
    need(len(relevant)==2,'unexpected graph dependency or alias')
    file_edge=[e for e in relevant if (e['parent'],e['child'],e.get('name'))==(ri,fi,'file')]
    frontend=[e for e in relevant if e['child']==ri and vertices[e['parent']].get('type')=='block-backend']
    need(len(file_edge)==len(frontend)==1,'raw/file/backend relation')
    for e in relevant:
        need(set(e['perm'])=={'consistent-read'},'graph write/resize or missing read permission')
    attached=[]
    for b in blocks:
        ins=b.get('inserted',{})
        need(isinstance(ins,dict),'inserted schema')
        if b.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE) or ins.get('node-name') in (RAW,FILE) or ins.get('file')==path: attached.append(b)
    need(len(attached)==1,'attachment absent/duplicate')
    b=attached[0]; ins=b.get('inserted',{})
    need(b.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE) and b.get('removable') is False,'attachment identity')
    need(ins.get('node-name')==RAW and ins.get('drv')=='raw' and ins.get('ro') is True and ins.get('file')==path and ins.get('image',{}).get('virtual-size')==N*SIZE,'attachment block metadata')
    need(ins.get('backing_file_depth')==0 and not ins.get('backing_file') and not ins.get('image',{}).get('backing-filename'),'attachment backing')
    return {'fixture_matches':True,'bdf':40,'unit':0,'sectors':N,'read_only_nodes':2,'graph_edges':2,'ata_access':False,'admission':False}
