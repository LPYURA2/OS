"""Replay genuine S833 REV1 initial QMP/UART + synthetic post-gate data.
No new genuine post-EBS snapshot claimed. Fake process tests socket protocol.
"""
import copy,json,hashlib,sys,tempfile
from pathlib import Path
from post_ebs_aperture_834 import READY,ACK,TOKEN,POST,post_extract,replay,capture
from qmp_aperture_833 import COMMANDS,extract
from irq_sources_830_verify import BACKEND

n=0
def bad(fn):
    global n
    try:fn()
    except ValueError:n+=1
    else:raise AssertionError('invalid fixture accepted')
def post(text):
    r=[]
    for i,((name,args),v) in enumerate(zip(POST,[{'running':True,'status':'running'},{},{'running':False,'status':'paused'},text,{}]),4):
        r.extend([{'tx':{'execute':name,'arguments':dict(args),'id':i}},{'rx':{'return':v,'id':i}}])
    return r

def fixture(p):
    initial=json.loads((p/'qmp-833.json').read_text());old=(p/'serial.log').read_bytes();assert old.rstrip().endswith(b'STYLEOS S816 PASS')
    prefix=old+READY+b'\n';raw=prefix+ACK+b'\n';t=dict(initial=initial,post=post(extract(initial)),release_hex=TOKEN.hex(),ready_sha256=hashlib.sha256(prefix).hexdigest())
    return raw,t

def main(root):
    global n
    passed=0
    for mode in BACKEND:
        p=root/mode
        if mode=='amd' and not (p/'serial.log').exists():continue
        raw,t=fixture(p);r=replay(raw,33,mode,t)
        assert r['stable_observed_apertures'] and r['initial_apertures']==r['post_ebs_apertures'] and not any(r[k] for k in ('physical_aperture_verified','ownership','mmio_authorized','dma_authorized','production_cr3_authorized'));passed+=1
        for rc in (0,32,None):bad(lambda rc=rc:replay(raw,rc,mode,t))
        for corrupt in (raw+b'\0',raw.replace(ACK,b'wrong'),raw.replace(READY,READY+b'\n'+READY),raw.replace(READY,b"SWAP").replace(ACK,READY).replace(b"SWAP",ACK),raw.replace(READY+b'\n',b'')):
            bad(lambda corrupt=corrupt:replay(corrupt,33,mode,t))
        for mut in (lambda t:t.update(release_hex='00'),lambda t:t.update(ready_sha256='0'*64),lambda t:t['post'][1]['rx']['return'].update(running=False),lambda t:t['post'][5]['rx']['return'].update(running=True),lambda t:t['post'][7]['rx'].update(id=77),lambda t:t['post'][8]['tx'].update(execute='quit'),lambda t:t['post'].append({'rx':{'event':'RESET'}})):
            q=copy.deepcopy(t);mut(q);bad(lambda q=q:replay(raw,33,mode,q))
        if mode!='disabled':
            q=copy.deepcopy(t);text=q['post'][7]['rx']['return'];name='amdvi-mmio' if mode=='amd' else 'intel_iommu';import re
            q['post'][7]['rx']['return'],changed=re.subn(r'([0-9a-f]{16})-([0-9a-f]{16})([^\n]*: '+name+r')',lambda m:m[1]+'-'+f'{int(m[2],16)-1:016x}'+m[3],text);assert changed
            bad(lambda:replay(raw,33,mode,q))
    print(f'{passed}/{passed} original initial QMP/UART + synthetic post-map replays PASS; {n}/{n} negative cases PASS')
    # Genuine local process/socket exercise with a fake QEMU, not an emulator.
    raw,t=fixture(root/'intel');old=(root/'intel'/'serial.log').read_bytes();tree=extract(t['initial'])
    fake=r'''import sys,socket,json,pathlib
args=sys.argv;serial=pathlib.Path(args[args.index('--log')+1]);qpath=args[args.index('-qmp')+1].split(':',1)[1].split(',',1)[0];gpath=args[args.index('-chardev')+1].split('path=',1)[1].split(',',1)[0]
q=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);q.bind(qpath);q.listen(1);g=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);g.bind(gpath);g.listen(1);c,_=q.accept();d,_=g.accept();f=c.makefile('rb')
def send(v):c.sendall((json.dumps(v)+'\n').encode())
send({'QMP':{}})
for i,v in enumerate([{}, {'running':False,'status':'prelaunch'}, TREE, {}, {'running':True,'status':'running'}, {}, {'running':False,'status':'paused'}, TREE, {}]):
 req=json.loads(f.readline());assert req['id']==i
 if i==5:send({'event':'STOP'})
 send({'return':v,'id':i})
 if i==3:serial.write_bytes(OLD+READY+b'\n')
assert d.recv(1)==b'\xa5';serial.write_bytes(OLD+READY+b'\n'+ACK+b'\n');c.close();d.close();q.close();g.close();sys.exit(33)
'''
    with tempfile.TemporaryDirectory() as temp:
        d=Path(temp);script=d/'fake.py';serial=d/'serial.log';script.write_text('TREE='+repr(tree)+'\nOLD='+repr(old)+'\nREADY='+repr(READY)+'\nACK='+repr(ACK)+'\n'+fake)
        assert capture([sys.executable,str(script),'--log',str(serial)],d,serial)==33
        r=replay(serial.read_bytes(),33,'intel',json.loads((d/'qmp-834.json').read_text()));assert r['stable_observed_apertures']
    print('1/1 fake process/socket lifecycle PASS (no genuine QEMU)')
if __name__=='__main__':main(Path(sys.argv[1]))
