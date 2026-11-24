import copy, tempfile
from pathlib import Path
from block_fixture_856_verify import verify_fixture,RAW,FILE,DEVICE,CONTROLLER
from sector_evidence_856 import fixture,N,SIZE

def run(seed):
    data=fixture(seed)
    with tempfile.TemporaryDirectory() as tmp:
        p=str(Path(tmp)/'fixture.raw'); Path(p).write_bytes(data)
        def node(name,drv):return {'node-name':name,'drv':drv,'ro':True,'file':p,'backing_file_depth':0,'image':{'virtual-size':N*SIZE}}
        raw=node(RAW,'raw'); file=node(FILE,'file')
        qtree='''bus: main-system-bus
  dev: q35-pcihost, id ""
    bus: pcie.0
      dev: piix3-ide, id "styleos-ide"
        bus: styleos-ide.0
        bus: styleos-ide.1
          dev: ide-hd, id "s856-disk"
            unit = 0 (0x0)
      dev: virtio-blk-pci, id "boot"
'''
        a={'blocks':[{'qdev':DEVICE,'removable':False,'inserted':copy.deepcopy(raw)},{'qdev':'boot','removable':False,'inserted':{'node-name':'boot','file':'boot.img'}}], 'nodes':[raw,file], 'qtree':qtree,'unit':0,'pci':[{'bus':0,'devices':[{'slot':5,'function':0,'qdev_id':CONTROLLER,'id':{'vendor':0x8086,'device':0x7010},'class_info':{'class':0x0101}}]}], 'graph':{'nodes':[{'id':1,'type':'block-backend','name':'disk'},{'id':2,'type':'block-driver','name':RAW},{'id':3,'type':'block-driver','name':FILE}], 'edges':[{'parent':1,'child':2,'name':'root','perm':['consistent-read']},{'parent':2,'child':3,'name':'file','perm':['consistent-read']}]},'path':p,'original':data,'current':data,'seed':seed}
        assert verify_fixture(**a)['admission'] is False
        b=copy.deepcopy(a); b['blocks'][0]['qdev']='/machine/peripheral/'+DEVICE; assert verify_fixture(**b)['fixture_matches']
        cases=[]
        def mutate(f):
            b=copy.deepcopy(a); f(b); cases.append(b)
        for key,value in [('unit',1),('unit',False),('current',data[:-1]),('original',data[:-1]),('seed',19 if seed==12 else 12),('path',p+'.wrong'),('blocks',[]),('nodes',[]),('pci',[]),('qtree','')]:mutate(lambda b,k=key,v=value:b.update({k:v}))
        for i in (0,1):
            for key,value in [('ro',False),('ro',1),('file',p+'.wrong'),('drv','qcow2'),('backing_file_depth',1),('backing_file','other')]:mutate(lambda b,i=i,k=key,v=value:b['nodes'][i].update({k:v}))
            mutate(lambda b,i=i:b['nodes'][i]['image'].update({'virtual-size':512}))
            mutate(lambda b,i=i:b['nodes'][i]['image'].update({'backing-filename':'other'}))
            mutate(lambda b,i=i:b['nodes'].append(copy.deepcopy(b['nodes'][i])))
        for key,value in [('qdev','foreign'),('removable',True)]:mutate(lambda b,k=key,v=value:b['blocks'][0].update({k:v}))
        for key,value in [('ro',False),('node-name',FILE),('drv','qcow2'),('file',p+'.wrong'),('backing_file_depth',1),('backing_file','other')]:mutate(lambda b,k=key,v=value:b['blocks'][0]['inserted'].update({k:v}))
        mutate(lambda b:b['blocks'][0]['inserted']['image'].update({'virtual-size':512}))
        mutate(lambda b:b['blocks'][0]['inserted']['image'].update({'backing-filename':'other'}))
        mutate(lambda b:b['blocks'].append(copy.deepcopy(b['blocks'][0])))
        mutate(lambda b:b['blocks'][1]['inserted'].update({'file':p}))
        for old,new in [('styleos-ide.1','styleos-ide.0'),('piix3-ide','ahci'),('ide-hd','ide-cd'),('id "s856-disk"','id "wrong"'),('      dev: virtio-blk-pci','          dev: ide-hd, id "extra"\n      dev: virtio-blk-pci'),('          dev: ide-hd','        dev: ide-hd')]:mutate(lambda b,o=old,n=new:b.update(qtree=b['qtree'].replace(o,n)))
        for key,value in [('slot',6),('function',1),('qdev_id','wrong'),('id',{'vendor':0x8086,'device':0x2922}),('class_info',{'class':0x0106})]:mutate(lambda b,k=key,v=value:b['pci'][0]['devices'][0].update({k:v}))
        mutate(lambda b:b['pci'][0]['devices'].append(copy.deepcopy(b['pci'][0]['devices'][0])))
        for i in (0,1):
            for perm in (['write'],['consistent-read','write'],['consistent-read','write-unchanged'],['consistent-read','resize'],[]):mutate(lambda b,i=i,v=perm:b['graph']['edges'][i].update(perm=v))
        mutate(lambda b:b['graph']['edges'][1].update(name='backing'))
        mutate(lambda b:b['graph']['edges'][1].update(child=2))
        mutate(lambda b:b['graph']['edges'].append(copy.deepcopy(b['graph']['edges'][0])))
        mutate(lambda b:b['graph']['nodes'].append(copy.deepcopy(b['graph']['nodes'][1])))
        mutate(lambda b:b['graph']['edges'].pop())
        mutate(lambda b:b['graph']['nodes'][2].update(name='unrelated-file'))
        for i,b in enumerate(cases):
            try:verify_fixture(**b)
            except ValueError:continue
            raise AssertionError(f'accepted mutation {i}')
        print(f'seed={seed}: 2 positive, {len(cases)} refusal PASS; synthetic QMP/HMP only')
        return len(cases)
print('total refusals',run(12)+run(19))
