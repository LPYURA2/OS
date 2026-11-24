"""Replay actual QMP identity from run 45491515720, artifact 14967550009."""
from copy import deepcopy
from block_fixture_856_verify import controller_identity
actual={'irq_pin':0,'bus':0,'qdev_id':'styleos-ide','slot':5,'class_info':{'class':257,'desc':'IDE controller'},'id':{'device':28688,'subsystem-vendor':6900,'vendor':32902,'subsystem':4352},'function':0,'regions':[{'bar':4,'size':16,'address':-1,'type':'io'}]}
assert controller_identity(actual)
minimal=deepcopy(actual);minimal['id']={'vendor':32902,'device':28688};assert controller_identity(minimal)
fields=[((), 'slot',5),((), 'function',0),(('id',),'vendor',32902),(('id',),'device',28688),(('class_info',),'class',257)]
count=0
for path,key,value in fields:
    for bad in (None,False,True,str(value),float(value),value+1):
        row=deepcopy(actual);obj=row
        for part in path:obj=obj[part]
        obj[key]=bad;assert not controller_identity(row);count+=1
    row=deepcopy(actual);obj=row
    for part in path:obj=obj[part]
    del obj[key];assert not controller_identity(row);count+=1
for key in ('id','class_info'):
    for bad in (None,[],0,'wrong'):
        row=deepcopy(actual);row[key]=bad;assert not controller_identity(row);count+=1
for bad in ('wrong','',None):
    row=deepcopy(actual);row['qdev_id']=bad;assert not controller_identity(row);count+=1
assert not controller_identity(None);count+=1
print(f'Actual QMP identity replay: 2 positive / {count} refusal PASS; no ATA admission')
