#!/usr/bin/env python3
"""Paused Q35 host probe: no guest execution and no storage admission."""
import argparse, hashlib, json, selectors, shutil, subprocess, time
from pathlib import Path
from block_fixture_856_verify import verify_fixture, CONTROLLER, DEVICE, RAW, FILE
from sector_evidence_856 import fixture

class QMP:
    def __init__(self, proc, transcript):
        self.proc, self.transcript, self.buf, self.seq = proc, transcript, b'', 0
    def receive(self):
        end=time.monotonic()+8
        with selectors.DefaultSelector() as sel:
            sel.register(self.proc.stdout, selectors.EVENT_READ)
            while b'\n' not in self.buf:
                remaining=end-time.monotonic()
                if remaining<=0 or not sel.select(remaining): raise TimeoutError('QMP response')
                data=self.proc.stdout.read(65536)
                if not data: raise RuntimeError('QMP EOF')
                self.buf+=data
                if len(self.buf)>8*1024*1024: raise ValueError('QMP response limit')
        row,self.buf=self.buf.split(b'\n',1)
        obj=json.loads(row); self.transcript.write(json.dumps({'rx':obj})+'\n'); self.transcript.flush()
        return obj
    def call(self, name, **args):
        self.seq+=1; command={'execute':name,'arguments':args,'id':self.seq}
        self.transcript.write(json.dumps({'tx':command})+'\n'); self.transcript.flush()
        self.proc.stdin.write((json.dumps(command)+'\n').encode()); self.proc.stdin.flush()
        for _ in range(64):
            reply=self.receive()
            if 'event' in reply:continue
            if reply.get('id')!=self.seq or 'return' not in reply:raise RuntimeError(f'{name}: {reply}')
            return reply['return']
        raise RuntimeError('QMP event limit')

def dump(path,obj):path.write_text(json.dumps(obj,indent=2)+'\n')
def probe(q, out, mode):
    out.mkdir(); path=out/'fixture.raw'; original=None; report={'mode':mode,'guest_executed':False,'ata_admitted':False,'status':'FAIL'}
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-nodefaults','-display','none','-monitor','none','-serial','none','-S','-qmp','stdio']
    if mode!='default':cmd+=['-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
    if mode=='readonly':
        original=fixture(19);path.write_bytes(original);path.chmod(0o444)
        cmd+=['-blockdev',json.dumps({'driver':'file','filename':str(path),'node-name':FILE,'read-only':True}),'-blockdev',json.dumps({'driver':'raw','file':FILE,'node-name':RAW,'read-only':True}),'-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={RAW}']
    dump(out/'command.json',cmd)
    try:
        with (out/'qemu.log').open('wb') as err,(out/'qmp.jsonl').open('w') as transcript:
            p=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,bufsize=0)
            try:
                m=QMP(p,transcript)
                if 'QMP' not in m.receive():raise ValueError('missing greeting')
                m.call('qmp_capabilities')
                observed={}
                for key,name,args in [('status','query-status',{}),('pci','query-pci',{}),('blocks','query-block',{}),('nodes','query-named-block-nodes',{}),('graph','x-debug-query-block-graph',{}),('qtree','human-monitor-command',{'command-line':'info qtree'}),('mtree','human-monitor-command',{'command-line':'info mtree -f -o'})]:
                    observed[key]=m.call(name,**args);dump(out/(key+'.json'),observed[key])
                if observed['status'].get('running') is not False:raise ValueError('guest not paused')
                devices=[d for b in observed['pci'] if b['bus']==0 for d in b['devices']]
                matches=[d for d in devices if d.get('qdev_id')==CONTROLLER]
                if mode=='default':
                    if matches:raise ValueError('unexpected explicit controller')
                elif len(matches)!=1 or matches[0]['slot']!=5 or matches[0]['function']!=0 or matches[0]['id']!={'vendor':0x8086,'device':0x7010}:raise ValueError('PIIX3 absent/wrong identity')
                if mode=='readonly':
                    unit=m.call('qom-get',path='/machine/peripheral/'+DEVICE,property='unit');dump(out/'unit.json',unit)
                    report['fixture']=verify_fixture(**{k:observed[k] for k in ('blocks','nodes','graph','qtree','pci')},unit=unit,path=path,original=original,current=path.read_bytes(),seed=19)
                report['status']='PASS';m.call('quit');p.wait(timeout=8)
                if p.returncode!=0:raise RuntimeError(f'QEMU exit {p.returncode}')
            finally:
                if p.poll() is None:p.kill();p.wait(timeout=8)
    except (OSError,RuntimeError,ValueError,TimeoutError,subprocess.TimeoutExpired,KeyError,TypeError) as exc:
        report.update(status='FAIL',error=str(exc))
    finally:
        if original is not None:
            actual=path.read_bytes();report['before_sha256']=hashlib.sha256(original).hexdigest();report['after_sha256']=hashlib.sha256(actual).hexdigest()
            if actual!=original:report.update(status='FAIL',error='fixture changed')
        dump(out/'result.json',report)
    return report

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    q=shutil.which('qemu-system-x86_64')
    if not q:raise RuntimeError('QEMU required')
    (out/'version.txt').write_text(subprocess.check_output([q,'--version'],text=True))
    results=[probe(q,out/m,m) for m in ('default','piix3','readonly')];dump(out/'results.json',results)
    for r in results:print(r['mode'],r['status'],r.get('error',''))
    return 0 if all(r['status']=='PASS' for r in results) else 1
if __name__=='__main__':raise SystemExit(main())
