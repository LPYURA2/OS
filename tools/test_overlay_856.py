from copy import deepcopy
from overlay_fixture_856 import *
from block_fixture_856_verify import CONTROLLER
base='/tmp/s856/base.raw';top='/tmp/s856/top.qcow2'
from test_pci_identity_856 import actual
pci=[{"bus":0,"devices":[actual]}]
nodes=[{'node-name':n,'drv':d,'ro':ro,'file':p,'image':{'virtual-size':LENGTH}} for n,d,ro,p in [(FILE,'file',True,base),(RAW,'raw',True,base),(TOPFILE,'file',False,top),(TOP,'qcow2',False,top)]]
qtree='''bus: main-system-bus
  dev: q35-pcihost, id ""
    bus: pcie.0
      dev: piix3-ide, id "styleos-ide"
        bus: styleos-ide.0
        bus: styleos-ide.1
          dev: ide-hd, id "s856-disk"
'''
o={'status':{'running':False},'unit':0,'pci':pci,'qtree':qtree,'nodes':nodes,'graph':{'nodes':[{'id':0,'type':'block-backend','name':'disk'}]+[{'id':i+1,'type':'block-driver','name':n['node-name']} for i,n in enumerate(nodes)],'edges':[{'parent':p,'child':c,'name':n,'perm':perm} for p,c,n,perm in [(0,4,'root',['consistent-read','write']),(4,3,'file',['consistent-read','write','resize']),(4,2,'backing',['consistent-read']),(2,1,'file',['consistent-read'])]]},'blocks':[{'qdev':DEVICE,'removable':False,'inserted':deepcopy(nodes[3])}],'stats':[{'qdev':DEVICE,'node-name':TOP,'stats':dict.fromkeys(ZERO,0)}]}
assert verify_attachment(o,base,top)['top_writable']
count=0
def reject(f):
    global count
    a=deepcopy(o);f(a)
    try:verify_attachment(a,base,top)
    except ValueError:count+=1;return
    raise AssertionError('accepted invalid fixture')
for v in [True,None,1]:reject(lambda a,v=v:a['status'].update(running=v))
for v in [True,False,1,None]:reject(lambda a,v=v:a.update(unit=v))
for i in range(4):
    for k,v in [('ro',not nodes[i]['ro']),('ro',0),('file','wrong'),('drv','wrong'),('node-name','wrong')]:reject(lambda a,i=i,k=k,v=v:a['nodes'][i].update({k:v}))
for i in (2,3):
    for perm in [['consistent-read','write'],['consistent-read','resize'],['write-unchanged'],[]]:reject(lambda a,i=i,perm=perm:a['graph']['edges'][i].update(perm=perm))
for i in range(4):reject(lambda a,i=i:a['graph']['edges'].pop(i))
for k in ZERO:
    for v in [1,None,False,-1]:reject(lambda a,k=k,v=v:a['stats'][0]['stats'].update({k:v}))
    reject(lambda a,k=k:a['stats'][0]['stats'].pop(k))
for k in ['failed_wr_operations','invalid_wr_operations','failed_unmap_operations','invalid_unmap_operations','failed_flush_operations','invalid_flush_operations']:reject(lambda a,k=k:a['stats'][0]['stats'].update({k:1}))
reject(lambda a:a['blocks'][0].update(qdev='foreign'));reject(lambda a:a['stats'][0].update(qdev='foreign'));reject(lambda a:a['stats'][0].update(**{'node-name':RAW}));reject(lambda a:a['nodes'].append(deepcopy(nodes[0])))
print('attachment: 1 positive /',count,'refusals PASS')
r=[{'start':0,'length':LENGTH,'depth':1,'data':True}];verify_map(r);verify_map([dict(r[0],length=512),dict(r[0],start=512,length=LENGTH-512)])
n=0
for k,vals in [('start',[1,-1,False]),('length',[0,-1,LENGTH+1,LENGTH-1]),('depth',[0,2,True]),('data',[False,1,None])]:
    for v in vals:
        try:verify_map([dict(r[0],**{k:v})])
        except ValueError:n+=1;continue
        raise AssertionError('map mutation accepted')
print('map: 2 positive /',n,'refusals PASS')
