"""S834 preflight: same-process stopped post-EBS QMP snapshot, no MMIO grants.
Integrated only in the separate S834 guest gate profile.
"""
import json, socket, subprocess, tempfile, time, hashlib
from pathlib import Path
from qmp_aperture_833 import COMMANDS, LIMIT, extract, aperture
from iommu_aperture_833 import correlate
from irq_sources_830_verify import verify, BACKEND, need

READY=b'STYLEOS S834 GATE post-ebs=yes wait-com2=yes'
ACK=b'STYLEOS S834 GATE release=yes'
TOKEN=b'\xa5'
POST=[('query-status',{}),('stop',{}),('query-status',{}),('human-monitor-command',{'command-line':'info mtree -f'}),('cont',{})]

def post_extract(trace):
    need(isinstance(trace,list) and 10<=len(trace)<=330,'post transcript length')
    results=[];p=0
    for i,(name,args) in enumerate(POST,4):
        need(p<len(trace) and trace[p]=={'tx':{'execute':name,'arguments':args,'id':i}},'post command sequence');p+=1
        count=0
        while p<len(trace) and set(trace[p])=={'rx'} and isinstance(trace[p]['rx'],dict) and 'event' in trace[p]['rx']:
            need(count<63,'post event limit');count+=1;p+=1
        need(p<len(trace) and set(trace[p])=={'rx'},'post response missing')
        v=trace[p]['rx'];p+=1
        need(isinstance(v,dict) and set(v)=={'return','id'} and v['id']==i,'post response identity');results.append(v['return'])
    need(p==len(trace) and results[1]=={} and results[4]=={},'post trailing data')
    need(isinstance(results[0],dict) and results[0].get('running') is True and results[0].get('status')=='running','guest not running at gate')
    need(isinstance(results[2],dict) and results[2].get('running') is False and results[2].get('status')=='paused','guest not stopped')
    need(isinstance(results[3],str) and 0<len(results[3])<=LIMIT,'post mtree extent')
    return results[3]

def ungate(raw):
    """Exact final two gate records after the pre-existing S816 PASS; preserve raw bytes."""
    need(isinstance(raw,bytes) and len(raw)<=LIMIT,'UART bounds')
    # Allow QEMU CRLF or LF, but no trailing garbage or duplicated gate markers.
    lines=raw.splitlines(keepends=True)
    norm=[v.rstrip(b'\r\n') for v in lines]
    need(len(lines)>=3 and norm[-3:]==[b'STYLEOS S816 PASS',READY,ACK] and all(v.endswith(b'\n') for v in lines[-3:]),'gate order')
    need(sum(v.startswith(b'STYLEOS S834 ') for v in norm)==2,'extra gate record')
    return b''.join(lines[:-2])

def replay(raw,rc,mode,trace):
    need(isinstance(trace,dict) and set(trace)=={'initial','post','release_hex','ready_sha256'},'transcript shape')
    need(trace['release_hex']==TOKEN.hex(),'wrong release')
    old=ungate(raw)
    need(hashlib.sha256(raw[:len(old)+len(raw[len(old):].splitlines(keepends=True)[0])]).hexdigest()==trace['ready_sha256'],'gate prefix mismatch')
    ok,d=verify(old,rc,mode);need(ok,'S830 parent UART')
    initial=extract(trace['initial']);post=post_extract(trace['post']);units=d['s828_inventory']['units']
    before=aperture(initial,BACKEND[mode],units);after=aperture(post,BACKEND[mode],units)
    need(before==after,'IOMMU aperture changed between snapshots')
    # Re-use S833's independent UEFI-map and resource conflict checks.
    report=correlate(old,rc,mode,trace['initial'])
    return dict(mode=mode,initial_apertures=before,post_ebs_apertures=after,stable_observed_apertures=True,detached_denied=report['detached_denied'],unchanged_candidates=report['unchanged_candidates'],physical_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False)

def capture(cmd,dst,serial):
    """Launch a stopped QEMU with a dedicated COM2 channel; no QMP write to memory.
    The runner must supply the separately built gate guest and COM1 file output.
    """
    trace={'initial':[],'post':[],'release_hex':None,'ready_sha256':None}
    with tempfile.TemporaryDirectory(prefix='s834-') as tmp,(dst/'qemu.log').open('wb') as log:
        path=str(Path(tmp)/'qmp.sock');gate=str(Path(tmp)/'gate.sock')
        args=cmd+['-S','-qmp',f'unix:{path},server=on,wait=off','-chardev',f'socket,id=s834gate,path={gate},server=on,wait=off','-serial','chardev:s834gate']
        with subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT) as proc:
            try:
                def connect(s,path):
                    deadline=time.monotonic()+10;s.settimeout(10)
                    while True:
                        try:s.connect(path);return
                        except (FileNotFoundError,ConnectionRefusedError):
                            need(proc.poll() is None and time.monotonic()<deadline,'socket startup failed');time.sleep(.025)
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as q,socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as g:
                    connect(q,path);connect(g,gate)
                    with q.makefile('rb') as f:
                        def rx(records):
                            line=f.readline(LIMIT+1);need(0<len(line)<=LIMIT,'QMP response extent');v=json.loads(line);need(isinstance(v,dict),'QMP response');records.append({'rx':v});return v
                        def commands(seq,start,records):
                            for i,(name,params) in enumerate(seq,start):
                                req={'execute':name,'arguments':params,'id':i};records.append({'tx':req});q.sendall((json.dumps(req)+'\n').encode())
                                for _ in range(64):
                                    v=rx(records)
                                    if 'event' in v:continue
                                    need(v.get('id')==i and 'return' in v and 'error' not in v,'QMP command refused');break
                                else:raise ValueError('QMP event limit')
                        need('QMP' in rx(trace['initial']),'greeting');commands(COMMANDS,0,trace['initial']);extract(trace['initial'])
                        deadline=time.monotonic()+30
                        while True:
                            raw=serial.read_bytes() if serial.exists() else b'';need(len(raw)<=LIMIT,'UART extent')
                            lines=raw.splitlines(keepends=True)
                            if lines and lines[-1].rstrip(b'\r\n')==READY and lines[-1].endswith(b'\n'):
                                need(len(lines)>=2 and lines[-2].rstrip(b'\r\n')==b'STYLEOS S816 PASS' and sum(v.startswith(b'STYLEOS S834 ') for v in lines)==1,'invalid ready marker')
                                break
                            need(proc.poll() is None and time.monotonic()<deadline,'guest gate timeout');time.sleep(.025)
                        trace['ready_sha256']=hashlib.sha256(raw).hexdigest();commands(POST,4,trace['post']);post_extract(trace['post'])
                        need(serial.read_bytes()==raw,'guest UART advanced before release')
                        g.sendall(TOKEN);trace['release_hex']=TOKEN.hex()
                rc=proc.wait(timeout=30);ungate(serial.read_bytes());need(rc==33,'guest exit')
            finally:
                if proc.poll() is None:proc.kill();proc.wait()
                (dst/'qmp-834.json').write_text(json.dumps(trace,indent=2)+'\n')
    return rc

def run(folder):
    from replay_dma_association_831 import replay as prior
    parent=prior(folder);meta=json.loads((folder/'results.json').read_text())
    need(meta.get('qmp_834_capture') is True,'capture profile absent')
    modes={}
    for mode,row in parent['modes'].items():
        if row['status']=='SKIPPED':modes[mode]={'status':'SKIPPED'};continue
        p=folder/mode;raw=(p/'serial-gated.log').read_bytes();trace=(p/'qmp-834.json').read_bytes();m=meta['modes'][mode]
        need(hashlib.sha256(raw).hexdigest()==m['gated_uart_sha256'] and hashlib.sha256(trace).hexdigest()==m['qmp_834_sha256'],'capture digest mismatch')
        need(ungate(raw)==(p/'serial.log').read_bytes(),'parent UART mismatch')
        r=replay(raw,33,mode,json.loads(trace));r['qmp_sha256']=m['qmp_834_sha256'];r['gated_uart_sha256']=m['gated_uart_sha256'];modes[mode]=r
        print(mode,r['initial_apertures'],r['post_ebs_apertures'],flush=True)
    return dict(stage=834,status='PASS',modes=modes,physical_aperture_verified=False,ownership=False,mmio_authorized=False,dma_authorized=False,production_cr3_authorized=False,foundation_complete=False)

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();r=run(a.input);a.output.mkdir(parents=True,exist_ok=True);(a.output/'results.json').write_text(json.dumps(r,indent=2)+'\n')
