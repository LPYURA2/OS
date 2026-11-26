"""Real UI stop: select PID, arm, cancel, prove continued execution, re-arm,
confirm, verify retirement and absence of subsequent execution. Original
catalog verifier still accounts for every raw hardware trap."""
import hashlib,json
from uart_receipts import complete_lines
from native_process_view_02 import decimal,EVENT
from native_catalog_865_verify import wait_hang_progress
ARM=b'STYLEOS PROCESSES ARM'
STOP=b'STYLEOS PROCESSES STOP'
FORCED=b'STYLEOS NATIVE FORCED-STOP '
TICK=b'STYLEOS NATIVE EVENT slot=0x0000000000000000 '
def verify_lines(lines):
    arms=[i for i,x in enumerate(lines) if x.startswith(ARM+b' ')]
    stops=[i for i,x in enumerate(lines) if x.startswith(STOP+b' ')]
    forced=[i for i,x in enumerate(lines) if x.startswith(FORCED)]
    if len(arms)!=2 or len(stops)!=1 or len(forced)!=1:raise ValueError('UI confirmation counts')
    a,b=arms;s=stops[0];r=forced[0]
    if not a<b<r<s or any(decimal(lines[i],ARM,(b'pid',))!=(1,) for i in arms) or decimal(lines[s],STOP,(b'pid',b'status',b'remaining'))!=(1,0,0):raise ValueError('UI stop identity/order')
    inputs=[(i,int(x.split(b' key=')[1].split()[0],16)) for i,x in enumerate(lines) if x.startswith(EVENT)]
    if not any(a<i<b and key==1 for i,key in inputs):raise ValueError('UI cancellation missing')
    if not any(x.startswith(TICK) and b' vector=0x0000000000000050 ' in x for x in lines[a:b]):raise ValueError('no progress while pending/cancelled')
    if not any(s<i and key==0x1c for i,key in inputs):raise ValueError('confirmation input missing')
    if any(x.startswith(TICK) for x in lines[s+1:]):raise ValueError('stopped process executed')
def exercise(d,proc,serial,folder,wait):
    def key(name):
        count=sum(x.startswith(EVENT) for x in complete_lines(serial.read_bytes()))
        d.key(name);wait(proc,serial,EVENT,count+1)
    key('down');key('delete')
    d.screen('stop-confirmation')
    key('esc');wait_hang_progress(proc,serial)
    key('delete');key('ret')
    wait(proc,serial,STOP+b' ',1)
    lines=complete_lines(serial.read_bytes())
    verify_lines(lines)
    d.screen('stop-completed')
    data={n:hashlib.sha256((folder/(n+'.ppm')).read_bytes()).hexdigest() for n in ('stop-confirmation','stop-completed')}
    (folder/'process-stop.json').write_text(json.dumps(data,indent=2)+'\n')
def verify(folder,lines):
    verify_lines(lines)
    data=json.loads((folder/'process-stop.json').read_text())
    for n,h in data.items():
        if hashlib.sha256((folder/(n+'.ppm')).read_bytes()).hexdigest()!=h:raise ValueError('stop screen digest')
