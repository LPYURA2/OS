"""Externally provisioned native catalog and independent QEMU CPL3 proof.
No ELF payload or test scheduling policy is linked into the ordinary kernel."""
import hashlib,re,struct,time
import json,zipfile
from irq_trace_accept_859 import HEADER
from runtime_mixed_863_verify import row

# One bad process and 31 independently admitted peers per boot.
# Payloads start after the known-good yield at offset 13.
FAULT_PROBES={
    'divide':(b'\x31\xc9\xf7\xf1',0,0x1000000f,0,0),
    'execute-nx':(b'\xb8\x00\x00\x00\x11\xff\xe0',14,0x11000000,0x15,0x11000000),
    'single-step':(b'\x9c\x48\x81\x0c\x24\x00\x01\x00\x00\x9d\x90\x0f\x0b',1,0x10000018,0,0),
    'forbidden-int':(b'\xcd\x81',13,0x1000000d,0x40a,0),
}

# Separate PID2 payload: consume initial queued sentinel, then wait for
# real pointer press/release and a key. Each stage polls only its own port.
# Final RAX=0 spin proves user code checked every returned wire field.
FOCUS_RETURNS=(0xf9,0x157,0x1d3,0x24f)
FOCUS_LOOP=0x2a3
def focus_payload(code):
    assert len(code)==0xe3 and code[0xdf:]==b'\xeb\xfe\x0f\x0b'
    code=bytearray(code)
    code[0xdf:0xe1]=b'\xeb\x02'
    failures=[]
    tests=(
        ((4,2),(12,2),(28,2),(32,0x5a,True),(33,2,True)),
        ((4,3),(12,3),(16,1),(20,0),(24,1),(28,8),(32,1),(36,1)),
        ((4,4),(12,3),(16,0),(20,0),(24,0),(28,8),(32,1),(36,1)),
        ((4,5),(12,1),(16,0x1e),(20,0),(24,1),(28,0)),
    )
    for expected,ret in zip(tests,FOCUS_RETURNS):
        again=len(code)
        code+=b'\xbb\0\0\0\0\xb9\x00\x01\x00\x12\xba\x40\0\0\0\xb8\x02\x01\0\0\xcd\x80'
        assert len(code)==ret
        code+=b'\x83\xf8\x06\x0f\x84'
        code+=struct.pack('<i',again-(len(code)+4))
        code+=b'\x85\xc0\x0f\x85'
        failures.append(len(code));code+=b'\0'*4
        code+=b'\xbf\x00\x01\x00\x12'
        for offset,value,*byte in expected:
            code+=(b'\x80' if byte else b'\x83')+b'\x7f'+bytes([offset,value])+b'\x0f\x85'
            failures.append(len(code));code+=b'\0'*4
    code+=b'\x31\xc0'
    assert len(code)==FOCUS_LOOP
    code+=b'\xeb\xfe'
    fail=len(code);code+=b'\x0f\x0b'
    for off in failures:struct.pack_into('<i',code,off,fail-(off+4))
    return bytes(code)

# PID2 replaces its tile after the verified real input sequence with a
# complete 160x120 retained frame. Refused requests must not change it.
FRAME_RETURNS=(0x307,0x326,0x34b)
FRAME_LOOP=0x356
def frame_payload(code):
    assert len(code)==FOCUS_LOOP+4 and code[FOCUS_LOOP:]==b'\xeb\xfe\x0f\x0b'
    code=bytearray(code)
    code[FOCUS_LOOP:FOCUS_LOOP+2]=b'\xeb\x02'
    code+=b'\xbf'+struct.pack('<I',0x12000200)
    for j,value in enumerate((0,0,160,120,0x234567,20,30,50,40,0x89abcd)):
        code+=(b'\xc7\x07' if not j else b'\xc7\x47'+bytes([4*j]))+struct.pack('<I',value)
    fixups=[]
    def call(va,ret,status):
        nonlocal code
        code+=b'\xbb\0\0\0\0\xb9'+struct.pack('<I',va)+b'\xba\x02\0\0\0\xb8\x03\x01\0\0\xcd\x80'
        assert len(code)==ret
        code+=(b'\x85\xc0' if not status else b'\x83\xf8'+bytes([status]))+b'\x0f\x85'
        fixups.append(len(code));code+=b'\0'*4
    call(0x12000ff0,FRAME_RETURNS[0],1)
    call(0x12000200,FRAME_RETURNS[1],0)
    code+=b'\xc7\x47\x08'+struct.pack('<I',159)
    call(0x12000200,FRAME_RETURNS[2],1)
    code+=b'\x31\xc0'
    assert len(code)==FRAME_LOOP
    code+=b'\xeb\xfe'
    fail=len(code);code+=b'\x0f\x0b'
    for off in fixups:struct.pack_into('<i',code,off,fail-(off+4))
    return bytes(code)

def provision_catalog(path,*,timer_load=False,bad_rsp=False,fault_probe=None,hang_load=False,forced_stop=False,packages=None,window_ipc=False,window_tile=False,window_input=False,window_focus=False,window_frame=False):
    if window_frame and not window_focus:raise ValueError('frame requires focus fixture')
    if window_focus and not window_input:raise ValueError('focus requires input fixture')
    if window_input and not window_tile:raise ValueError('input requires tile fixture')
    if window_tile and not window_ipc:raise ValueError('tile requires IPC fixture')
    if window_ipc and (not forced_stop or packages is None):raise ValueError('window IPC requires separate package fixture')
    if packages is not None:
        if not forced_stop:raise ValueError('package fixture requires forced-stop profile')
        packages.mkdir(parents=True,exist_ok=False)
        from tea_package import pack,inspect
        package_rows=[]
    if forced_stop and (timer_load or bad_rsp or fault_probe or hang_load):raise ValueError('separate forced stop workload')
    if hang_load and (timer_load or bad_rsp or fault_probe):raise ValueError('separate infinite-loop workload')
    if fault_probe and (timer_load or bad_rsp or fault_probe not in FAULT_PROBES):raise ValueError('separate fault probe')
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
            if (hang_load and i==0) or (forced_stop and i<2):
                # Valid user context, no syscall/fault/exit after yield.
                if window_tile and i<2:
                    # Write four distinct pixels to the real private data page.
                    code+=b'\xbf'+struct.pack('<I',0x12000000)
                    for j in range(4):
                        code+=(b'\xc7\x07' if not j else b'\xc7\x47'+bytes([4*j]))+struct.pack('<I',0xc10000+i+1+(j<<8))
                    def tile_call(va):
                        return b'\xbb\x00\x00\x00\x00\xb9'+struct.pack('<I',va)+b'\xba\x02\x00\x02\x00\xb8\x01\x01\x00\x00\xcd\x80'
                    code+=tile_call(0x12000ff8)
                    assert len(code)==0x43
                    code+=b'\x83\xf8\x01\x75\x1e'
                    code+=tile_call(0x12000000)
                    assert len(code)==0x5e
                    code+=b'\x85\xc0\x75\x04\x31\xc0\xeb\xfe\x0f\x0b'
                    assert len(code)==0x68
                    if window_input:
                        # Keep original tile failure UD at 0x66. On success
                        # jump over it, refuse a cross-page input destination,
                        # then read the same event into private data memory.
                        code=code[:0x64]+b'\xeb\x02'+code[0x66:]
                        fixups=[]
                        def check(test):
                            nonlocal code
                            code+=test+b'\x0f\x85'
                            fixups.append(len(code));code+=b'\0'*4
                        def input_call(va):
                            return b'\xbb\0\0\0\0\xb9'+struct.pack('<I',va)+b'\xba\x40\0\0\0\xb8\x02\x01\0\0\xcd\x80'
                        code+=input_call(0x12000fe0)
                        assert len(code)==0x7e
                        check(b'\x83\xf8\x01')
                        code+=input_call(0x12000100)
                        assert len(code)==0x9d
                        check(b'\x85\xc0')
                        code+=b'\xbf'+struct.pack('<I',0x12000100)
                        check(b'\x83\x3f\x01')
                        check(b'\x83\x7f\x04\x01')
                        check(b'\x83\x7f\x0c\x02')
                        check(b'\x83\x7f\x1c\x02')
                        check(b'\x66\x81\x7f\x20\x5a\x70')
                        code+=b'\x31\xc0'
                        assert len(code)==0xdf
                        code+=b'\xeb\xfe'
                        fail=len(code);code+=b'\x0f\x0b'
                        code=bytearray(code)
                        for off in fixups:struct.pack_into('<i',code,off,fail-(off+4))
                        code=bytes(code)
                        if window_focus and i==1:code=focus_payload(code)
                        if window_frame and i==1:code=frame_payload(code)
                elif window_ipc and i<2:
                    # Real CPL3 register-only call. Failed return faults;
                    # success loops with RAX=0 at code offset 0x24.
                    code+=b'\xbb\x00\x00\x00\x00\xb9'+struct.pack('<I',0xb10000+i+1)+b'\xb8\x00\x01\x00\x00\xcd\x80\x85\xc0\x75\x04\x31\xc0\xeb\xfe\x0f\x0b'
                else:code+=b'\xeb\xfe'
            elif window_frame and i==2:
                # Read a real queued key. Ordinary caller gets zero and exits.
                # Epoch 1 faults before approval. Epoch 2 sees the same key,
                # decides refresh, checks the committed epoch, then exits.
                code+=b'\xb8\x05\x01\x00\x00\xcd\x80'
                assert len(code)==0x14
                code+=b'\x48\x85\xc0\x74\x22\x48\x89\xc3\x48\xc1\xeb\x20'
                code+=b'\x83\xf8\x44\x75\x1d\x83\xfb\x01\x74\x18'
                code+=b'\xb9\x01\x00\x00\x00\xb8\x06\x01\x00\x00\xcd\x80'
                assert len(code)==0x36
                code+=b'\x83\xf8\x02\x75\x07\xb8\x02\x00\x00\x00\xcd\x80\x0f\x0b'
                assert len(code)==0x44
            elif fault_probe and i==0:
                code+=FAULT_PROBES[fault_probe][0]+b'\x0f\x0b'
            elif bad_rsp and i<3:
                # After one normal yield, set an invalid RSP, then exercise
                # UD, INT80 and real timer separately. No forced interrupt.
                code+=b'\xbc'+struct.pack('<I',0x12000000)
                code+=(b'\x0f\x0b',b'\xcd\x80',b'\xeb\xfe')[i]
            else:code+=b'\x0f\x0b' if i%8==7 else b'\xb8\x02\x00\x00\x00\xcd\x80\x0f\x0b'
            blob=bytearray(4096)+code
            blob[:7]=b'\x7fELF\x02\x01\x01'
            struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,0x10000000,64,0,0,64,56,1,0,0,0)
            struct.pack_into('<IIQQQQQQ',blob,64,1,5,4096,0x10000000,0,len(code),4096,4096)
            if packages is not None and i<2:
                # Standalone native packages, independently validated on host.
                # The existing guest catalog admits the verified ELF payload,
                # not ZIP bytes. No claim of native .tea installation here.
                elf=packages/f'probe-{i}.elf'
                tea=packages/f'probe-{i}.tea'
                elf.write_bytes(blob)
                manifest=pack(tea,elf,f'org.styleos.probe{i}',f'Boundary probe {i}','0.1.0','x86_64')
                if inspect(tea)!=manifest:raise ValueError('package manifest mismatch')
                with zipfile.ZipFile(tea) as archive: payload=archive.read(manifest['entry'])
                if payload!=blob or hashlib.sha256(payload).hexdigest()!=manifest['sha256']:
                    raise ValueError('package payload mismatch')
                blob=payload
                package_rows.append(dict(pid_expected=i+1,package=tea.name,package_sha256=hashlib.sha256(tea.read_bytes()).hexdigest(),manifest=manifest))
            lba=3072+16*i
            struct.pack_into('<II32s8s',catalog,16+48*i,lba,len(blob),hashlib.sha256(blob).digest(),(b'stop00' if forced_stop and i==0 else b'frame1' if window_frame and i==1 else b'focus1' if window_focus and i==1 else b'input1' if window_input and i==1 else f'app{i:02}'.encode()))
            disk.seek(lba*512);disk.write(blob.ljust(8192,b'\0'))
        disk.seek(3000*512);disk.write(catalog)
    if packages is not None:
        (packages/'manifest-evidence.json').write_text(json.dumps(package_rows,indent=2)+'\n')
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

def verify_nx_cr2(block,expected):
    # QEMU prints CR2 in the exception header and register dump.
    values=re.findall(r'\bCR2=([0-9a-fA-F]+)\b',block,re.M)
    header=re.findall(r'^\s*\d+: v=0e [^\n]*\bCR2=([0-9a-fA-F]+)\b',block,re.M)
    regs=re.findall(r'^CR0=[0-9a-fA-F]+ CR2=([0-9a-fA-F]+) CR3=',block,re.M)
    if len(values)!=2 or len(header)!=1 or len(regs)!=1 or any(int(x,16)!=expected for x in values+header+regs):
        raise ValueError('NX fault address evidence')

def complete_uart_lines(raw):
    # Actual QEMU UART is CRLF; ignore an unfinished final line only.
    return raw[:raw.rfind(b'\n')+1].splitlines()

def hang_progress(lines):
    retired=[j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S865 FX-RETIRE ')]
    if len(retired)!=31:return False
    post=lines[retired[-1]+1:]
    return sum(x.startswith(b'STYLEOS NATIVE EVENT slot=0x0000000000000000 ') and b' vector=0x0000000000000050 ' in x for x in post)>=8

def wait_hang_progress(proc,serial,seconds=60):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        raw=serial.read_bytes() if serial.exists() else b''
        lines=complete_uart_lines(raw)
        if b' FAIL ' in raw or proc.poll() is not None:raise RuntimeError('hang audit guest stopped')
        if hang_progress(lines):return
        time.sleep(.03)
    raise RuntimeError('no peer completion and sustained preemption')

def wait_hang_after_input(proc,serial,seconds=20):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        raw=serial.read_bytes();lines=complete_uart_lines(raw)
        if b' FAIL ' in raw or proc.poll() is not None:raise RuntimeError('hang audit guest stopped')
        inputs=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863 EVENT ') and b' key=0x0000000000000040 ' in x]
        if inputs and sum(x.startswith(b'STYLEOS NATIVE EVENT slot=0x0000000000000000 ') and b' vector=0x0000000000000050 ' in x for x in lines[inputs[-1]+1:])>=2:return
        time.sleep(.03)
    raise RuntimeError('no timer progress after shell input')

def verify_catalog(lines,trace,obs,*,timer_load=False,bad_rsp=False,fault_probe=None,hang_load=False,ui_stop=False):
    if ui_stop and not hang_load:raise ValueError('UI stop requires hang workload')
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
    if hang_load:
        if ui_stop:
            stops=records(b'STYLEOS NATIVE FORCED-STOP',('slot','pid','gen','root'))
            if empty or [tuple(x.values()) for x in stops]!=[(0,1,1,roots[0])]:raise ValueError('UI forced stop identity')
            at=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE FORCED-STOP '))
            if not any(x.startswith(b'STYLEOS S865 FX-RETIRE ') and b' gen=0x0000000000000001' in x for x in lines[:at]):raise ValueError('UI stop retirement')
            if any(x.startswith(b'STYLEOS NATIVE EVENT ') for x in lines[at+1:]):raise ValueError('execution after stop')
            retire0=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S865 FX-RETIRE ') and b' gen=0x0000000000000001' in x)
            if not hang_progress(lines[:retire0]):raise ValueError('pre-stop workload not proved')
        elif empty or not hang_progress(lines):raise ValueError('infinite workload not resident or peers not reclaimed')
    elif len(empty)!=1 or tuple(empty[0].values())!=(0,len(events)) or len(events)<64:raise ValueError('native final reclamation')
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
        malformed=bad_rsp and i in (0,1,2) and event['sp']==0x12000000
        probe_fault=bool(fault_probe and i==0 and event['vector']==FAULT_PROBES[fault_probe][1])
        if not 0<=i<32 or event['pid']!=i+1 or event['root']!=roots[i] or event['sp']!=(0x12000000 if malformed else 0x11001000) or event['flags']&0x202!=0x202 or event['error']!=(FAULT_PROBES[fault_probe][3] if probe_fault else 0):raise ValueError('native event identity')
        expected_state=1 if event['vector']==0x50 or event['vector']==128 and event['rax']==1 else 4 if event['vector']==128 and event['rax']==2 else 5 if event['vector']==6 else -1
        if malformed or probe_fault:expected_state=5
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
        if hang_load and i==0:
            if (seq[0]['vector'],seq[0]['rip'],seq[0]['rax'],seq[0]['state'])!=(128,0x1000000d,1,1) or len(seq)<9 or any((x['vector'],x['rip'],x['rax'],x['state'])!=(0x50,0x1000000d,0,1) for x in seq[1:]):
                raise ValueError('infinite process must remain READY and preemptible')
            continue
        if seq[-1]['state'] not in (4,5) or any(x['state']!=1 for x in seq[:-1]):raise ValueError('terminal lifecycle')
        actual=[(x['vector'],x['rip'],x['rax']) for x in seq if x['vector']!=0x50]
        off=9 if timer_load else 0
        expected=[(128,0x1000000d+off,1),(6,0x1000000d+off,0) if i%8==7 else (128,0x10000014+off,2)]
        if bad_rsp and i<3:
            expected=[(128,0x1000000d,1)]
            if i==0:expected.append((6,0x10000012,0))
            if i==1:expected.append((128,0x10000014,0))
            terminal=seq[-1]
            if (terminal['vector'],terminal['rip'],terminal['rax'],terminal['sp'],terminal['state'])!=((6,128,0x50)[i],(0x10000012,0x10000014,0x10000012)[i],0,0x12000000,5):raise ValueError('malformed RSP terminal evidence')
        if fault_probe and i==0:
            _,vector,rip,error,rax=FAULT_PROBES[fault_probe]
            expected=[(128,0x1000000d,1),(vector,rip,rax)]
            if seq[-1]['error']!=error or seq[-1]['state']!=5:raise ValueError('probe terminal evidence')
        if actual!=expected:raise ValueError('native yield/terminal '+str((i,actual)))
    if fault_probe:
        fault_index=next(j for j,x in enumerate(events) if x['slot']==0 and x['state']==5)
        if not any(x['slot']!=0 and x['state']==4 for x in events[fault_index+1:]):raise ValueError('no peer exit after probe fault')
    if bad_rsp:
        bad_index=max(j for j,x in enumerate(events) if x['slot']<3 and x['state']==5)
        if not any(x['slot']>=3 and x['state']==4 for x in events[bad_index+1:]):raise ValueError('no surviving process after malformed RSP')
    retired=records(b'STYLEOS S865 FX-RETIRE',('id','root','gen'))
    if sorted((x['id'],x['root'],x['gen']) for x in retired)!=[(i,roots[i],i+1) for i in range(1 if hang_load and not ui_stop else 0,32)]:raise ValueError('per-owner retirement')
    if hang_load:
        # The observation ends after an input receipt and at least two more
        # completed slices. The loop intentionally survives host teardown.
        inputs=[j for j,x in enumerate(lines) if x.startswith(b'STYLEOS S863 EVENT ') and b' key=0x0000000000000040 ' in x]
        if not inputs:raise ValueError('no responsive shell')
        fields=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')
        lang=row(lines[inputs[-1]],b'STYLEOS S863 EVENT',fields)
        prev=next(row(x,b'STYLEOS S863 EVENT',fields) for x in reversed(lines[:inputs[-1]]) if x.startswith(b'STYLEOS S863 EVENT '))
        if lang['root']!=root or lang['mouse'] or lang['ru']==prev['ru'] or not lang['writes']:raise ValueError('shell did not respond to language switch')
        if not ui_stop and sum(x.startswith(b'STYLEOS NATIVE EVENT slot=0x0000000000000000 ') and b' vector=0x0000000000000050 ' in x for x in lines[inputs[-1]+1:])<2:raise ValueError('no continued IRQ after input')
    # Independent hardware trace: match every user event, never accept
    # software receipts alone. Kernel drain records are independently bound.
    headers=list(HEADER.finditer(trace));result=[];started=False;index=0;denied_int=None;denied_count=0;tail=0
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
        if denied_int is not None:
            if (vec,err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(13,0x40a,0,3,0x5b,0x1000000d,0x1000000d,0x63,0x11001000,roots[0]) or int(m[1])!=denied_int+1:
                raise ValueError('denied INT must immediately cause matching GP')
            denied_int=None
        elif fault_probe=='forbidden-int' and vec==0x81:
            if (err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(0,1,3,0x5b,0x1000000d,0x1000000d,0x63,0x11001000,roots[0]) or denied_count:
                raise ValueError('unexpected software INT attempt')
            denied_int=int(m[1]);denied_count+=1;continue
        if cpl==0:
            if drain_index>=len(drains):raise ValueError('unaccounted kernel interrupt')
            dr=drains[drain_index];drain_index+=1
            if (vec,err,soft,cr3,rip,pc)!=(0x50,0,0,root,dr['pc'],dr['pc']):raise ValueError('native timer drain')
            continue
        if index>=len(events):
            # Host can stop after the last complete UART receipt but before
            # the next IRQ has committed. Only one such in-flight slice is
            # allowed; it must be the exact same live loop and child root.
            if not hang_load or ui_stop or tail or (vec,err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(0x50,0,0,3,0x5b,0x1000000d,0x1000000d,0x63,0x11001000,roots[0]) or one(r'\bR12=([0-9a-fA-F]+)\b')!=1 or one(r'\bRAX=([0-9a-fA-F]+)\b')!=0:
                raise ValueError('extra CPL3 trap')
            tail+=1;continue
        ev=events[index];index+=1
        actual_pc=ev['rip']-2*(ev['vector']==128)
        if (vec,err,soft,cpl,cs,rip,pc,ss,sp,cr3)!=(ev['vector'],ev['error'],int(ev['vector']==128),3,0x5b,actual_pc,actual_pc,0x63,ev['sp'],ev['root']):raise ValueError('native raw CPU mismatch')
        if one(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b')!=ev['flags'] or one(r'\bRAX=([0-9a-fA-F]+)\b')!=ev['rax'] or one(r'\bR12=([0-9a-fA-F]+)\b')!=(0 if ev['vector']==0x50 and ev['rip']==0x10000000 else ev['slot']+1):raise ValueError('native raw registers')
        if fault_probe=='execute-nx' and ev['slot']==0 and vec==14:verify_nx_cr2(block,0x11000000)
        result.append((int(m[1]),ev))
    if denied_int is not None or denied_count!=int(fault_probe=='forbidden-int'):raise ValueError('missing denied INT evidence')
    if not started or index!=len(events) or drain_index!=len(drains):raise ValueError('native CPU count')
    return result
