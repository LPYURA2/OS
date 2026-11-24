"""Synthetic flat maps and QMP protocol. No genuine new QEMU execution."""
import sys,json,tempfile
from pathlib import Path
from qmp_aperture_833 import COMMANDS,extract,aperture,capture
from iommu_aperture_833 import correlate,union
from irq_sources_830_verify import BACKEND,verify

def row(a,b,name,kind='i/o'):
    return f'  {a:016x}-{b-1:016x} (prio 0, {kind}): {name}\n'
def mtree(base=None,size=4096,name='intel_iommu'):
    return 'FlatView #0\n AS "memory", root: system\n AS "cpu-memory-0", root: system\n Root memory region: system\n'+row(0,0x100000,'ram','ram')+(row(base,base+size,name) if base is not None else '')+'FlatView #1\n AS "I/O", root: io\n Root memory region: io\n'+row(0,16,'port')
def trace(text):
    r=[{'rx':{'QMP':{}}}]
    for i,((name,args),value) in enumerate(zip(COMMANDS,[{}, {'running':False,'status':'prelaunch'},text,{}])):
        r += [{'tx':{'execute':name,'arguments':dict(args),'id':i}},{'rx':{'return':value,'id':i}}]
    return r

def main(folder):
    n=0
    def bad(fn):
        nonlocal n
        try:fn()
        except ValueError:n+=1
        else:raise AssertionError('bad fixture accepted')
    b=0xfed90000;u=[{'base':b}];a=0xfed80000;v=[{'base':a}];t=mtree(b)
    assert aperture(extract(trace(t)),1,u)==[[b,b+4096]];n+=1
    assert aperture(mtree(a,16384,'amdvi-mmio'),2,v)==[[a,a+16384]];n+=1
    assert aperture(mtree(),0,[])==[];n+=1
    for txt,backend,units in [(t,0,[]),(t,2,v),(t,1,[]),(mtree(),1,u),(mtree(b+4096),1,u),(mtree(b,4095),1,u),(mtree(b,2**21),1,u),(t.replace('(prio 0, i/o): intel_iommu','(prio 0, ram): intel_iommu'),1,u),(t.replace('AS "memory"','AS "other"'),1,u),(t+t,1,u),(t.replace('FlatView #1',row(b+1,b+2,'duplicate')+'FlatView #1'),1,u),(t.replace('FlatView #1',row(b+4096,b+8192,'intel_iommu')+'FlatView #1'),1,u),(t.replace('FlatView #1',' invalid row\nFlatView #1'),1,u)]:bad(lambda txt=txt,backend=backend,units=units:aperture(txt,backend,units))
    import copy
    for i in (0,1,2,3,4,5,6,7,8):
        r=trace(t);r.pop(i);bad(lambda r=r:extract(r))
    for mutate in (lambda r:r[4]['rx']['return'].update(running=True),lambda r:r[4]['rx']['return'].update(status='running'),lambda r:r[5]['tx']['arguments'].update({'command-line':'info registers'}),lambda r:r[6]['rx'].update(id=99),lambda r:r[8]['rx'].update(error={}),lambda r:r.append({'rx':{'event':'RESET'}})):
        r=trace(t);mutate(r);bad(lambda r=r:extract(r))
    r=trace(t);r.insert(8,{'rx':{'event':'RESUME'}});assert extract(r)==t;n+=1
    assert union([[5,10],[2,5],[12,15],[6,7]])==[[2,10],[12,15]];n+=1
    # Fake QEMU subprocess speaks the same local protocol, not hardware.
    fake='''import sys,socket,json\np=sys.argv[sys.argv.index('-qmp')+1].split(':',1)[1].split(',',1)[0]\ns=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);s.bind(p);s.listen(1);c,_=s.accept()\nf=c.makefile('rb')\ndef send(v):c.sendall((json.dumps(v)+'\\n').encode())\nsend({'QMP':{}})\nfor i,value in enumerate([{}, {'running':False,'status':'prelaunch'}, %r, {}]):\n q=json.loads(f.readline());assert q['id']==i\n if i==3:send({'event':'RESUME'})\n send({'id':i,'return':value})\nc.close();s.close();sys.exit(33)\n'''%t
    with tempfile.TemporaryDirectory() as tmp:
        p=Path(tmp);script=p/'fake.py';script.write_text(fake);assert capture([sys.executable,str(script)],p)==33;assert extract(json.loads((p/'qmp-833.json').read_text()))==t;n+=1
    print(f'{n}/{n} synthetic monitor/parser scenarios PASS')
    count=0
    for mode,backend in BACKEND.items():
        p=folder/mode/'serial.log'
        if mode=='amd' and not p.exists():continue
        raw=p.read_bytes();ok,d=verify(raw,33,mode);assert ok
        unit=d['s828_inventory']['units'];txt=mtree() if not unit else mtree(unit[0]['base'],16384 if backend==2 else 4096,'amdvi-mmio' if backend==2 else 'intel_iommu')
        result=correlate(raw,33,mode,trace(txt));assert not result['physical_aperture_verified'] and not result['ownership'] and not result['dma_authorized']
        for entry in result['initial_qemu_model_apertures']:assert entry['uefi_status']=='GAP'
        count+=1
        for rc in (0,32,None):bad(lambda rc=rc:correlate(raw,rc,mode,trace(txt)))
    print(f'{count}/{count} original UART replays with SYNTHETIC monitor data PASS; no new QEMU')
if __name__=='__main__':main(Path(sys.argv[1]))
