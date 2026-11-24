#!/usr/bin/env python3
"""No guest execution; no permission grant. Preserve actual failure evidence."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path
from block_fixture_856_verify import CONTROLLER, DEVICE, RAW, FILE
from overlay_fixture_856 import TOP, TOPFILE, verify_attachment, verify_map
from probe_storage_fixture_856 import QMP, dump
from sector_evidence_856 import fixture

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);args=p.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    base=out/'fixture.raw';top=out/'disposable.qcow2';data=fixture(19);base.write_bytes(data);base.chmod(0o444)
    q=shutil.which('qemu-system-x86_64');img=shutil.which('qemu-img');report={'status':'FAIL','guest_executed':False,'ata_admitted':False,'top_writable':True}
    def image(*args):return subprocess.check_output([img,*args],stderr=subprocess.STDOUT,timeout=20)
    def map_check(tag):
        rows=json.loads(image('map','--output=json','-f','qcow2',str(top)));dump(out/(tag+'.json'),rows);verify_map(rows)
    try:
        if not q or not img:raise RuntimeError('QEMU and qemu-img required')
        (out/'version.txt').write_bytes(subprocess.check_output([q,'--version']))
        (out/'create.log').write_bytes(image('create','-f','qcow2','-F','raw','-b',str(base),str(top)))
        map_check('map-before');image('compare','-f','raw','-F','qcow2',str(base),str(top))
        cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-nodefaults','-display','none','-monitor','none','-serial','none','-S','-qmp','stdio','-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
        for n in ({'driver':'file','filename':str(base),'node-name':FILE,'read-only':True},{'driver':'raw','file':FILE,'node-name':RAW,'read-only':True},{'driver':'file','filename':str(top),'node-name':TOPFILE,'read-only':False},{'driver':'qcow2','file':TOPFILE,'backing':RAW,'node-name':TOP,'read-only':False}):cmd+=['-blockdev',json.dumps(n)]
        cmd+=['-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={TOP}'];dump(out/'command.json',cmd)
        with (out/'qemu.log').open('wb') as err,(out/'qmp.jsonl').open('w') as transcript:
            proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,bufsize=0)
            try:
                m=QMP(proc,transcript)
                if 'QMP' not in m.receive():raise ValueError('greeting')
                m.call('qmp_capabilities');o={}
                for key,method,kw in [('status','query-status',{}),('pci','query-pci',{}),('blocks','query-block',{}),('nodes','query-named-block-nodes',{}),('graph','x-debug-query-block-graph',{}),('qtree','human-monitor-command',{'command-line':'info qtree'}),('unit','qom-get',{'path':'/machine/peripheral/'+DEVICE,'property':'unit'}),('stats','query-blockstats',{'query-nodes':False})]:
                    o[key]=m.call(method,**kw);dump(out/(key+'.json'),o[key])
                report['proof']=verify_attachment(o,base,top);m.call('quit');proc.wait(timeout=8)
                if proc.returncode:raise RuntimeError('QEMU exit '+str(proc.returncode))
            finally:
                if proc.poll() is None:proc.kill();proc.wait(timeout=8)
        map_check('map-after');(out/'compare.log').write_bytes(image('compare','-f','raw','-F','qcow2',str(base),str(top)));(out/'check.log').write_bytes(image('check','-f','qcow2',str(top)))
        report['status']='PASS'
    except Exception as exc:
        report['error']=str(exc)
        if isinstance(exc,subprocess.CalledProcessError):report['command_output']=exc.output.decode(errors='replace')[-4096:]
    finally:
        actual=base.read_bytes();report['before_sha256']=hashlib.sha256(data).hexdigest();report['after_sha256']=hashlib.sha256(actual).hexdigest()
        if actual!=data:report.update(status='FAIL',error='immutable backing changed')
        dump(out/'result.json',report);print(report)
    return int(report['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
