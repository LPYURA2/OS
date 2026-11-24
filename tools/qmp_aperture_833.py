"""S833: capture QEMU's initial CPU physical flat map before the SAME guest runs.
No guest MMIO transactions, no claim of physical-machine aperture/ownership.
"""
import json, re, socket, subprocess, tempfile, time
from pathlib import Path
from uefi_map_826 import require

COMMANDS=[('qmp_capabilities',{}),('query-status',{}),('human-monitor-command',{'command-line':'info mtree -f'}),('cont',{})]
LIMIT=2*1024*1024
NAMES={1:'intel_iommu',2:'amdvi-mmio'}

def capture(cmd,dst):
    """Start stopped, query only the monitor's map, resume, collect debug exit."""
    trace=[]
    with tempfile.TemporaryDirectory(prefix='s833-') as tmp, (dst/'qemu.log').open('wb') as log:
        path=str(Path(tmp)/'qmp.sock')
        with subprocess.Popen(cmd+['-S','-qmp',f'unix:{path},server=on,wait=off'],stdout=log,stderr=subprocess.STDOUT) as p:
            try:
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
                    s.settimeout(10);deadline=time.monotonic()+10
                    while True:
                        try:s.connect(path);break
                        except (FileNotFoundError,ConnectionRefusedError):
                            if p.poll() is not None or time.monotonic()>=deadline:raise RuntimeError('QMP startup failed')
                            time.sleep(.025)
                    with s.makefile('rb') as f:
                        def rx():
                            line=f.readline(LIMIT+1);require(0<len(line)<=LIMIT,'QMP response length');v=json.loads(line);require(isinstance(v,dict),'QMP response');trace.append({'rx':v});return v
                        require('QMP' in rx(),'QMP greeting')
                        for i,(name,args) in enumerate(COMMANDS):
                            req={'execute':name,'arguments':args,'id':i};trace.append({'tx':req});s.sendall((json.dumps(req)+'\n').encode())
                            for _ in range(64):
                                v=rx()
                                if 'event' in v:continue
                                require(v.get('id')==i and 'return' in v and 'error' not in v,'QMP command refused');break
                            else:raise ValueError('QMP event limit')
                        extract(trace)
                rc=p.wait(timeout=30)
            finally:
                if p.poll() is None:p.kill();p.wait()
                (dst/'qmp-833.json').write_text(json.dumps(trace,indent=2)+'\n')
    return rc

def extract(trace):
    require(isinstance(trace,list) and 9<=len(trace)<=265,'QMP trace length')
    require(set(trace[0])=={'rx'} and isinstance(trace[0]['rx'].get('QMP'),dict),'QMP greeting')
    p=1;results=[]
    for i,(name,args) in enumerate(COMMANDS):
        require(p<len(trace) and trace[p]=={'tx':{'execute':name,'arguments':args,'id':i}},'QMP command sequence');p+=1
        events=0
        while p<len(trace) and set(trace[p])=={'rx'} and 'event' in trace[p]['rx']:
            require(events<63,'QMP event limit');events+=1;p+=1
        require(p<len(trace) and set(trace[p])=={'rx'},'QMP response missing')
        v=trace[p]['rx'];p+=1;require(set(v)=={'return','id'} and v['id']==i,'QMP response identity');results.append(v['return'])
    require(p==len(trace) and results[0]=={} and results[3]=={},'QMP trailing data')
    require(isinstance(results[1],dict) and results[1].get('running') is False and results[1].get('status') in ('prelaunch','paused'),'VM was not stopped')
    require(isinstance(results[2],str) and 0<len(results[2])<=LIMIT,'mtree extent')
    return results[2]

def aperture(text,backend,units):
    # A flat view is a resolved address space, not the nested container tree.
    blocks=re.split(r'(?m)^FlatView #[0-9]+\s*$',text)
    matches=[b for b in blocks[1:] if re.search(r'(?m)^\s*AS "memory", root: system\s*$',b)]
    require(len(matches)==1,'CPU memory flat view missing/ambiguous')
    rows=[]
    for line in matches[0].splitlines():
        line=line.strip()
        if not line or line.startswith('AS ') or line=='Root memory region: system':continue
        m=re.fullmatch(r'([0-9a-fA-F]{16})-([0-9a-fA-F]{16}) \(prio -?[0-9]+, ([^)]*)\): (.+)',line)
        require(m,'unsupported flat view row '+line[:100]);a,b=int(m[1],16),int(m[2],16)+1
        require(a<b<=2**64,'bad flat extent');rows.append((a,b,m[3],m[4]))
    require(rows and len(rows)<=8192 and all(a[1]<=b[0] for a,b in zip(rows,rows[1:])),'flat view ordering/overlap')
    found=[v for v in rows if v[3] in NAMES.values()]
    if backend==0:
        require(not found and not units,'unexpected controller');return []
    require(backend in NAMES and len(units)==len(found)==1,'unsupported controller count')
    a,b,kind,name=found[0];u=units[0]
    require(name==NAMES[backend] and kind=='i/o' and a==u['base'] and a%4096==b%4096==0 and 4096<=b-a<=1024*1024,'controller aperture mismatch')
    return [[a,b]]
