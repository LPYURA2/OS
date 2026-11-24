from copy import deepcopy
from pathlib import Path
import sys
from legacy_ports_857 import *
p=Path(sys.argv[1]);q=Path(sys.argv[2]);d=pair_dir(p/'default');a=pair_dir(p/'piix3');r=load(q,'ready-');pre=load(q,'prelaunch-')
print(verify_pair(d,a));print(verify_ready(pre));print(verify_ready(r));count=0

def reject(fn):
    global count
    try:fn()
    except ValueError:count+=1;return
    raise AssertionError('unexpected acceptance')
for i in range(4):
    lo,hi=PORTS[i];row=f'{lo:016x}-{hi:016x} (prio 0, i/o): {DESC}'
    assert row in a['mtree'] and row in r['mtree']
    for bad in (row.replace('prio 0','prio 1'),row.replace(DESC,'foreign'),row.replace(f'{hi:016x}',f'{hi+1:016x}'),row.replace(f'{lo:016x}',f'{lo+1:016x}')):
        reject(lambda:verify_pair(d,dict(a,mtree=a['mtree'].replace(row,bad))))
        reject(lambda:verify_ready(dict(r,mtree=r['mtree'].replace(row,bad))))
    # Inject occupied range into no-controller snapshot, preserving order via actual map
    reject(lambda:verify_pair(dict(d,mtree=a['mtree']),a))
for change in ('slot','function','qdev_id'):
    for source in (a,r):
        b=deepcopy(source);dev=next(x for x in b['pci'][0]['devices'] if x.get('qdev_id')=='styleos-ide');dev[change]=1 if change!='qdev_id' else 'wrong'
        reject(lambda:verify_pair(d,b) if source is a else verify_ready(b))
for x in (a,r):
    b=deepcopy(x);b['status']['running']=True;reject(lambda:verify_pair(d,b) if x is a else verify_ready(b))
    b=deepcopy(x);b['pci'][0]['devices'].append(deepcopy(b['pci'][0]['devices'][0]));reject(lambda:verify_pair(d,b) if x is a else verify_ready(b))
reject(lambda:verify_pair(d,dict(a,command=a['command']+['-S'])))
reject(lambda:verify_ready(dict(r,qtree=r['qtree'].replace('bus: styleos-ide.1','bus: styleos-ide.0'))))
reject(lambda:verify_pair(d,dict(a,qtree=a['qtree'].replace('piix3-ide','foreign'))))
print('3 actual evidence checks,',count,'mutations rejected; no ATA admission')
