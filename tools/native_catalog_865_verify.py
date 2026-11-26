"""Externally provisioned native catalog and independent QEMU CPL3 proof.
No ELF payload or test scheduling policy is linked into the ordinary kernel."""
import hashlib,re,struct,time
from irq_trace_accept_859 import HEADER
from runtime_mixed_863_verify import row

def provision_catalog(path,*,timer_load=False,bad_rsp=False):
    if timer_load and bad_rsp:raise ValueError("separate audit workload")
    catalog=bytearray(2048)
    struct.pack_into('<8sII',catalog,0,b'STYLNAT1',1,32)
    with path.open('r+b') as disk:
        for i in range(32):
            # r12 identifies independent executable bytes. Yield then exit;
            # every eighth image deliberately raises UD after yielding.
            code=b'\x41\xbc'+struct.pack('<I',i+1)
            # A finite external CPU-bound loop, no forced timer or test hook.
            # All 32 residents must suffer real LAPIC preemption before yield.
            if timer_load:code+=b'\xb9'+struct.pack('<I',100_000_000)+b'\xff\xc9\x75\xfc'
            code+=b'\xb8\x01\x00\x00\x00\xcd\x80'
            if bad_rsp and i==0:
                # After one normal yield, move RSP outside its user stack and
                # execute UD2. No kernel-side injection or altered trap policy.
                code+=b'\xbc'+struct.pack('<I',0x12000000)+b'\x0f\x0b'
            else:code+=b'\x0f\x0b' if i%8==7 else b'\xb8\x02\x00\x00\x00\xcd\x80\x0f\x0b'
            blob=bytearray(4096)+code
            blob[:7]=b'\x7fELF\x02\x01\x01'
            struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,0x10000000,64,0,0,64,56,1,0,0,0)
            struct.pack_into('<IIQQQQQQ',blob,64,1,5,4096,0x10000000,0,len(code),4096,4096)
            lba=3072+16*i
            struct.pack_into('<II32s8s',catalog,16+48*i,lba,len(blob),hashlib.sha256(blob).digest(),f'app{i:02}'.encode())
            disk.seek(lba*512);disk.write(blob.ljust(8192,b'\0'))
        disk.seek(3000*512);disk.write(catalog)
    return hashlib.sha256(catalog).hexdigest()

def wait_native_empty(proc,serial,seconds):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        lines=serial.read_bytes().splitlines() if serial.exists() else []
        failures=[x for x in lines if x.startswith((b'STYLEOS S848 FAIL ',b'STYLEOS S850 FAIL ',b'STYLEOS RUNTIME FAIL '))]
        if failures:raise RuntimeError('native fail-stop: '+repr(failures[-1]))
        if any(x.startswith(b'STYLEOS NATIVE EMPTY ') for x in lines):return
        if proc.poll() is not None:raise RuntimeError('native guest exited')
        time.sleep(.03)
    raise RuntimeError('native completion timeout')

def verify_catalog(lines,trace,obs,*,timer_load=False,bad_rsp=False):
    root=obs['active']['root']
    def records(tag,keys):
        return [row(x,tag,keys) for x in lines if x.startswith(tag+b' ')]
    admissions=records(b'STYLEOS NATIVE ADMIT',('slot','pid','gen','root','entry'))
    batches=records(b'STYLEOS NATIVE CATALOG',('valid','requested','accepted','resident'))
    events=records(b'STYLEOS NATIVE EVENT',('slot','pid','root','vector','error','rip','sp','flags','rax','state'))
    empty=records(b'STYLEOS NATIVE EMPTY',('used','dispatches'))
    if len(admissions)!=32 or len(batches)!=1 or tuple(batches[0].values())!=(1,32,32,32):raise ValueError('32 simultaneous residents')
    roots=[a['root'] for a in admissions]
    if len(set(roots))!=32 or root in roots:raise ValueError('distinct roots')
    for i,a in enumerate(admissions):
        if (a['slot'],a['pid'],a['gen'],a['entry'])!=(i,i+1,i+1,0x10000000) or not a['root'] or a['root']&4095:raise ValueError('native admission')
    if len(empty)!=1 or tuple(empty[0].values())!=(0,len(events)) or len(events)<64:raise ValueError('native final reclamation')
    aidx=[j for j,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE ADMIT ')]
    eidx=[j for j,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE EVENT ')]
    if not aidx[-1]<next(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE CATALOG '))<eidx[0]:raise ValueError('resident before execution')
    irqs=records(b'STYLEOS NATIVE IRQ-RETIRE',('slot','root','initial','current','lvt','isr','irr'))
    expected_irqs=[(x['slot'],x['root'],0,0,0x10050,0,0) for x in events if x['vector']==0x50]
    if [tuple(x.values()) for x in irqs]!=expected_irqs:raise ValueError('native IRQ retirement readback')
    irq_lines=[j for j,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE IRQ-RETIRE ')]
    previous=-1;irq_index=0
    for event,index in zip(events,eidx):
        if event['vector']==0x50:
            if not previous<irq_lines[irq_index]<index:raise ValueError('IRQ retirement order')
            irq_index+=1
        previous=index
    per=[[] for _ in roots]
    for event in events:
        i=event['slot']
        if not 0<=i<32 or event['pid']!=i+1 or event['root']!=roots[i] or event['sp']!=(0x12000000 if bad_rsp and i==0 and event['vector']==6 else 0x11001000) or event['flags']&0x202!=0x202 or event['error']:raise ValueError('native event identity')
        expected_state=1 if event['vector']==0x50 or event['vector']==128 and event['rax']==1 else 4 if event['vector']==128 and event['rax']==2 else 5 if event['vector']==6 else -1
        if event['state']!=expected_state:raise ValueError('native lifecycle state')
        per[i].append(event)
    if [x['slot'] for x in events[:32]]!=list(range(32)):raise ValueError('first round fairness')
    for i,seq in enumerate(per):
        if not seq:raise ValueError('missing native execution')
        if timer_load:
            first_yield=next((j for j,x in enumerate(seq) if x['vector']==128 and x['rax']==1),None)
            if first_yield is None or first_yield<1 or not all(x['vector']==0x50 for x in seq[:first_yield]):
                raise ValueError('no actual preemption before yield for slot '+str(i))
            if any(x['vector']==0x50 and x['rip']-0x10000000 not in (0,6,11,13,15,20,22,27,29) for x in seq):
                raise ValueError('timer outside external instruction boundary')
        if seq[-1]['state'] not in (4,5) or any(x['state']!=1 for x in seq[:-1]):raise ValueError('terminal lifecycle')
        actual=[(x['vector'],x['rip'],x['rax']) for x in seq if x['vector']!=0x50]
        off=9 if timer_load else 0
        expected=[(128,0x1000000d+off,1),(6,0x1000000d+off,0) if i%8==7 else (128,0x10000014+off,2)]
        if bad_rsp and i==0:expected=[(128,0x1000000d,1),(6,0x10000012,0)]
        if actual!=expected:raise ValueError('native yield/terminal '+str((i,actual)))
    if bad_rsp:
        bad_index=next(j for j,x in enumerate(events) if x['slot']==0 and x['vector']==6)
        if not any(x['slot']!=0 and x['state']==4 for x in events[bad_index+1:]):raise ValueError('no surviving process after malformed RSP')
    retired=records(b'STYLEOS S865 FX-RETIRE',('id','root','gen'))
    if sorted((x['id'],x['root'],x['gen']) for x in retired)!=[(i,roots[i],i+1) for i in range(32)]:raise ValueError('per-owner retirement')
    # Independent hardware trace: match every user event, never accept
    # software receipts alone. Kernel drain records are independently bound.
    headers=list(HEADER.finditer(trace));result=[];started=False;index=0
    drains=records(b'STYLEOS S865 TIMER-DRAIN',('id','safe','pc','vector'));drain_index=0
    for j,m in enumerate(headers):
        block=trace[m.start():headers[j+1].start() if j+1<len(headers) else len(trace)]
        def one(p):
            values=re.findall(p,block,re.M)
            if len(values)!=1:raise ValueError('native CPU '+p)
            return int(values[0],16)
        cr3=one(r'\bCR3=([0-9a-fA-F]+)\b')
        if not started:
            if obs.get('boot_probes') is False:
                if cr3 not in [root,*roots]:continue
                started=True
            else:
                if cr3!=root:continue
                if tuple(int(x,16) for x in m.groups()[1:5])!=(3,0,1,0):raise ValueError('native safe breakpoint')
                if int(m[7],16)!=obs['bp']['rip']-1:raise ValueError('native breakpoint site')
                started=True;continue
        vec,err,soft,cpl,cs,rip,pc,ss,sp=map(lambda x:int(x,16),m.groups()[1:])
        if cpl==0:
            if drain_index>=len(drains):raise ValueError('unaccounted kernel interrupt')
            dr=drains[drain_index];drain_index+=1
            if (vec,err,soft,cr3,rip,pc)!=(0x50,0,0,root,dr['pc'],dr['pc']):raise ValueError('native timer drain')
            continue
        if index>=len(events):raise ValueError('extra CPL3 trap')
        ev=events[index];index+=1
        actual_pc=ev['rip']-2*(ev['vector']==128)
        if (vec,err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(ev['vector'],ev['error'],int(ev['vector']==128),3,0x5b,actual_pc,actual_pc,0x63,ev['sp'],ev['root']):raise ValueError('native raw CPU mismatch')
        if one(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b')!=ev['flags'] or one(r'\bRAX=([0-9a-fA-F]+)\b')!=ev['rax'] or one(r'\bR12=([0-9a-fA-F]+)\b')!=(0 if ev['vector']==0x50 and ev['rip']==0x10000000 else ev['slot']+1):raise ValueError('native raw registers')
        result.append((int(m[1]),ev))
    if not started or index!=len(events) or drain_index!=len(drains):raise ValueError('native CPU count')
    return result
