"""Observe the real F8 read-only snapshot at a scheduler boundary.
Bind the live PID/tick count to preceding actual native event/retire receipts.
The unchanged native_catalog verifier separately binds those to the CPU trace.
"""
import hashlib,json,re
from test_desktop_v0 import Frame
from uart_receipts import complete_lines
from runtime_mixed_863_verify import row
HEADER=b'STYLEOS PROCESSES SNAPSHOT'
ROW=b'STYLEOS PROCESSES ROW'
END=b'STYLEOS PROCESSES END'
EVENT=b'STYLEOS S863 EVENT '
def decimal(line,tag,keys):
    m=re.fullmatch(re.escape(tag)+b' '+b' '.join(k+b'=([0-9]+)' for k in keys),line)
    if not m:raise ValueError('process view receipt schema')
    return tuple(int(v) for v in m.groups())
def verify_snapshot(lines,*,live):
    starts=[i for i,x in enumerate(lines) if x.startswith(HEADER+b' ')]
    ends=[i for i,x in enumerate(lines) if x==END]
    ordinary=not live and b'STYLEOS SHELL PROVIDER READY' in lines
    if ordinary:
        if not 1<=len(starts)<=2 or len(ends)!=len(starts):raise ValueError('ordinary snapshot count')
        for start,end in zip(starts,ends):
            if end!=start+1 or decimal(lines[start],HEADER,(b'valid',b'total',b'flags'))!=(1,0,3):
                raise ValueError('ordinary process snapshot after controller cleanup')
    elif len(starts)!=1 or len(ends)!=1 or ends[0]<=starts[0]:raise ValueError('process view receipt count')
    start,end=starts[0],ends[0]
    if decimal(lines[start],HEADER,(b'valid',b'total',b'flags'))!=(1,int(live),3):raise ValueError('process view validity/count/flags')
    values=[decimal(x,ROW,(b'pid',b'group',b'state',b'ticks',b'bytes')) for x in lines[start+1:end]]
    if len(values)!=int(live):raise ValueError('process view row count')
    if live:
        events=[row(x,b'STYLEOS NATIVE EVENT',('slot','pid','root','vector','error','rip','sp','flags','rax','state')) for x in lines[:start] if x.startswith(b'STYLEOS NATIVE EVENT ')]
        own=[e for e in events if e['pid']==1]
        retired=[x for x in lines[:start] if x.startswith(b'STYLEOS S865 FX-RETIRE ')]
        if len(retired)!=31 or not own or own[-1]['state']!=1:raise ValueError('process view expected one surviving process')
        ticks=sum(e['vector']==0x50 for e in own)
        if ticks<8 or values!=[(1,2,1,ticks,12288)]:
            # Native user may own a real WS_APP_WINDOW; group 1 then applies.
            if ticks<8 or values!=[(1,1,1,ticks,12288)]:raise ValueError('process view mismatched live accounting')
    inputs=[x for x in lines[end+1:] if x.startswith(EVENT)]
    if not live and b'STYLEOS SHELL PROVIDER READY' in lines:
        # Ordinary F8 input is deferred; actual render follows the snapshot.
        if not any(x.startswith(EVENT) and (b' key=0x0000000000000042 ' in x or (b' key=0x0000000000000001 ' in x and b' modifiers=0x0000000000000003 ' in x)) for x in lines[:start]) or b'STYLEOS SHELL ACTION RENDERED' not in lines[end+1:]:
            raise ValueError('deferred F8 render not observed')
    elif not inputs or b' key=0x0000000000000042 ' not in inputs[0] or b' active=0x0000000000000002 ' not in inputs[0] or b' writes=0x0000000000000000' in inputs[0]:raise ValueError('F8 render not observed')
    return values
def exercise(d,proc,serial,folder,wait):
    prior=complete_lines(serial.read_bytes())
    count=sum(x.startswith(EVENT) for x in prior)
    d.key('f8');wait(proc,serial,EVENT,count+1)
    lines=complete_lines(serial.read_bytes())
    values=verify_snapshot(lines,live=True)
    frame=d.screen('live-process-view')
    old=Frame(folder/'after-denied-fixture.ppm')
    if frame.pixels==old.pixels:raise ValueError('process panel not rendered')
    (folder/'live-process-view.json').write_text(json.dumps({'rows':values,'sha256':hashlib.sha256((folder/'live-process-view.ppm').read_bytes()).hexdigest()},indent=2)+'\n')
def verify(folder,lines,root,*,live):
    values=verify_snapshot(lines,live=live)
    data=json.loads((folder/'live-process-view.json').read_text())
    if data['rows']!=[list(x) for x in values] or data['sha256']!=hashlib.sha256((folder/'live-process-view.ppm').read_bytes()).hexdigest():raise ValueError('process view artifact mismatch')
