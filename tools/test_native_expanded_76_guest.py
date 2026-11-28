"""Stage 76: actual cross-page input and expanded forced stop."""
import argparse
import json
import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
from typing import NamedTuple
from test_deep_drop_guest import Desktop, Run, firmware_path, digest, records, complete_lines, CONTROLLER, DEVICE
from make_desktop_data import provision
from irq_trace_accept_859 import HEADER

ROOT=Path(__file__).resolve().parents[1]
CODE=0x10000000
DATA=0x12000000
STACK=0x11000000
P=4096
EVENT=b'STYLEOS NATIVE EVENT'
KEYS=('slot','pid','root','vector','error','rip','sp','flags','rax','state')
ADMIT=b'STYLEOS WIDE75 ADMIT'
LEGACY=b'STYLEOS NATIVE ADMIT'
RELEASE=b'STYLEOS WIDE75 RELEASE'
IRQ=b'STYLEOS NATIVE IRQ-RETIRE'
IRQ_KEYS=('slot','root','initial','current','lvt','isr','irr')
MIN_SLICES=2

class Sites(NamedTuple):
    wait: tuple
    yielded: int
    terminal: int

if not __debug__:
    raise RuntimeError('guest acceptance requires Python assertions')

def wait_for_release(code,base,offset):
    """PAUSE; CMP byte [RSI+offset],0; JE back to PAUSE."""
    start=base+len(code)
    code.extend(b'\xf3\x90\x80\xbe'+struct.pack('<I',offset)+b'\0\x74\xf5')
    return (start,start+2,start+9)

def elf(code,wide):
    if wide:
        blob=bytearray(5*P)
        entry=CODE+P
        struct.pack_into('<IIQQQQQQ',blob,64,1,5,P,CODE,0,P+len(code),2*P,P)
        struct.pack_into('<IIQQQQQQ',blob,120,1,6,3*P,DATA,0,P+8,3*P,P)
        blob[2*P:2*P+len(code)]=code
        struct.pack_into('<Q',blob,4*P,0x12345678)
    else:
        blob=bytearray(P+len(code));entry=CODE
        struct.pack_into('<IIQQQQQQ',blob,64,1,5,P,CODE,0,len(code),P,P)
        blob[P:]=code
    blob[:7]=b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,entry,64,0,0,64,56,2 if wide else 1,0,0,0)
    return bytes(blob)

def payload(fault):
    code=bytearray();fix=[]
    def emit(b):code.extend(b)
    def fail_if_ne():emit(b'\x0f\x85');fix.append(len(code));emit(bytes(4))
    # Read initialized data in page 2, check zero BSS in page 3, write
    # distinct marker there and keep a private value on the second stack page.
    emit(b'\xbe'+struct.pack('<I',DATA+P))
    emit(b'\x48\x81\x3e\x78\x56\x34\x12');fail_if_ne()
    emit(b'\x48\x83\xbe\x00\x10\x00\x00\x00');fail_if_ne()
    marker=struct.pack('<I',0x1234567+int(fault))
    sentinel=struct.pack('<I',0x7654321+int(fault))
    emit(b'\x48\xc7\x86\x00\x10\x00\x00'+marker)
    emit(b'\x68'+sentinel)
    wait=wait_for_release(code,CODE+P,P+8)
    emit(b'\xb8\x01\x00\x00\x00\xcd\x80')
    yield_pc=CODE+P+len(code)
    emit(b'\x85\xc0');fail_if_ne()
    emit(b'\x5b\x81\xfb'+sentinel);fail_if_ne()
    emit(b'\x48\x81\xbe\x00\x10\x00\x00'+marker);fail_if_ne()
    if fault:
        terminal=CODE+P+len(code);emit(b'\x0f\x0b')
    else:
        emit(b'\xb8\x02\x00\x00\x00\xcd\x80');terminal=CODE+P+len(code)
    failure=len(code);emit(b'\x0f\x0b')
    for off in fix:struct.pack_into('<i',code,off,failure-off-4)
    return elf(code,True),Sites(wait,yield_pc,terminal)

def legacy_payload():
    call_yield=b'\xb8\x01\0\0\0\xcd\x80'
    code=bytearray(call_yield)
    first_yield=CODE+len(code)
    code.extend(b'\xbe'+struct.pack('<I',DATA))
    wait=wait_for_release(code,CODE,8)
    # Remain runnable while both expanded apps exercise their terminal paths.
    code.extend(call_yield*3+b'\xb8\x02\0\0\0\xcd\x80')
    terminal=CODE+len(code)
    code.extend(b'\x0f\x0b')
    return elf(code,False),Sites(wait,first_yield,terminal)

def ipc_payload():
    code=bytearray();fix=[]
    def emit(b):code.extend(b)
    def check(value):
        emit(b'\x83\xf8'+bytes([value])+b'\x0f\x85');fix.append(len(code));emit(bytes(4))
    # Mark the last mapped bytes before requesting an invalid 64-byte span.
    emit(b'\xc7\x04\x25'+struct.pack('<II',DATA+3*P-4,0x11223344))
    def input_call(address):
        emit(b'\xb8\x02\x01\0\0\x31\xdb\xb9'+struct.pack('<I',address)+
             b'\xba\x40\0\0\0\xcd\x80')
        return CODE+P+len(code)
    refused=input_call(DATA+3*P-32);check(1)
    good=input_call(DATA+P-32);check(0)
    emit(b'\xc7\x04\x25'+struct.pack('<II',DATA+2*P,0x66778899))
    emit(b'\xbe'+struct.pack('<I',DATA+P))
    wait=wait_for_release(code,CODE+P,P+8)
    failure=len(code);emit(b'\x0f\x0b')
    for off in fix:struct.pack_into('<i',code,off,failure-off-4)
    return elf(code,True),Sites(wait,refused,good)

def prepare(out,target=None,*,target_name='ipc76'):
    disk=out/'expanded.raw'
    provision(disk)
    good,good_sites=ipc_payload() if target is None else target
    bad,bad_sites=payload(False)
    peer,peer_sites=legacy_payload()
    catalog=bytearray(2048)
    struct.pack_into('<8sII',catalog,0,b'STYLNAT1',1,3)
    with disk.open('r+b') as f:
        f.seek(4095*512);f.write(b'STYLEOS-S864-PROVISIONED-V1'.ljust(512,b'\0'))
        for i,(name,lba,blob) in enumerate(((target_name,3072,good),('wide76',3120,bad),('peer76',3168,peer))):
            struct.pack_into('<II32s8s',catalog,16+i*48,lba,len(blob),hashlib.sha256(blob).digest(),name.encode())
            f.seek(lba*512);f.write(blob)
            (out/(name+'.elf')).write_bytes(blob)
        f.seek(3000*512);f.write(catalog)
    return disk,hashlib.sha256(catalog).hexdigest(),(good_sites,bad_sites,peer_sites)

def admissions(lines):
    wide=[v for _,v in records(lines,ADMIT,('pid','gen','root','bytes'))]
    peer=[v for _,v in records(lines,LEGACY,('slot','pid','gen','root','entry'))]
    assert len(wide)==2 and all(v['bytes']==7*P for v in wide)
    assert len(peer)==1 and peer[0]['entry']==CODE
    apps=wide+peer
    for key in ('pid','gen','root'):
        assert len({a[key] for a in apps})==3 and all(a[key] for a in apps)
    return apps

def wait_slices(events,app,sites):
    return [i for i,e in enumerate(events) if e['pid']==app['pid'] and e['root']==app['root']
            and e['vector']==0x50 and e['state']==1 and e['rip'] in sites.wait]

def verify(trace,lines,apps,pcs,*,sdk_stack=False,extra_ops=()):
    entries=list(records(lines,EVENT,KEYS));events=[e for _,e in entries]
    a,b,z=apps
    stop=list(records(lines,b'STYLEOS WIDE76 STOP',('pid','gen','root','peer','legacy','resident','bytes','wire','queue')))
    bound=list(records(lines,b'STYLEOS WIDE76 BOUND',('pid','window','peer','peer-window')))
    forced=list(records(lines,b'STYLEOS NATIVE FORCED-STOP',('slot','pid','gen','root')))
    assert len(stop)==len(bound)==len(forced)==1
    at,s=stop[0];fi,f=forced[0];bi,bind=bound[0]
    assert s==dict(pid=a['pid'],gen=a['gen'],root=a['root'],peer=b['pid'],legacy=z['pid'],resident=2,bytes=10*P,wire=64,queue=1)
    assert f['pid']==a['pid'] and f['gen']==a['gen'] and f['root']==a['root'] and bi<fi<at
    assert bind['pid']==a['pid'] and bind['peer']==b['pid'] and bind['window']!=bind['peer-window']
    calls=list(records(lines,b'STYLEOS NATIVE INPUT',('pid','gen','root','window','va','bytes','status','seq')))
    assert len(calls)==2
    for (i,call),va,status,seq in zip(calls,(DATA+3*P-32,DATA+P-32),(1,0),(0,1)):
        assert bi<i<fi and call==dict(pid=a['pid'],gen=a['gen'],root=a['root'],window=0,va=va,bytes=64,status=status,seq=seq)
    ae=[e for i,e in entries if e['pid']==a['pid']]
    ipc=[e for e in ae if e['vector']==128]
    assert [(e['rax'],e['rip']) for e in ipc]==[(0x102,pcs[0].yielded),(0x102,pcs[0].terminal)]+[(op,pcs[0].terminal) for op in extra_ops]
    assert all(i<fi and e['state']==1 for i,e in entries if e['pid']==a['pid'])
    assert all(e['root']==a['root'] and CODE+P<=e['rip']<CODE+2*P and (STACK+P<e['sp']<=STACK+2*P if sdk_stack else e['sp']==STACK+2*P) for e in ae)
    before=[e for i,e in entries if i<fi]
    counts=[len(wait_slices(before,app,site)) for app,site in zip(apps,pcs)]
    assert all(n>=MIN_SLICES for n in counts)
    for app,site in zip(apps[1:],pcs[1:]):
        after=[e for i,e in entries if i>at and e['pid']==app['pid']]
        assert after and after[-1]['vector']==128 and after[-1]['rax']==2 and after[-1]['state']==4 and after[-1]['rip']==site.terminal
        assert any(e['vector']==128 and e['rax']==1 and e['state']==1 for e in after)
        assert all(e['root']==app['root'] for e in after)
    assert all(e['pid'] in (a['pid'],b['pid'],z['pid']) and e['vector'] in (128,0x50) and e['error']==0 for e in events)
    irq=[tuple(v.values()) for _,v in records(lines,IRQ,IRQ_KEYS)]
    assert irq==[(e['slot'],e['root'],0,0,0x10050,0,0) for e in events if e['vector']==0x50]
    empty=list(records(lines,b'STYLEOS NATIVE EMPTY',('used','dispatches')))
    assert len(empty)==1 and empty[0][0]>entries[-1][0] and empty[0][1]==dict(used=0,dispatches=len(events))
    data=trace.read_text();heads=list(HEADER.finditer(data));real=[]
    for i,head in enumerate(heads):
        if int(head[5])!=3:continue
        chunk=data[head.start():heads[i+1].start() if i+1<len(heads) else len(data)]
        root=int(re.search(r'\bCR3=([0-9a-fA-F]+)',chunk)[1],16)
        vec=int(head[2],16)
        assert int(head[6],16)==0x5b and int(head[9],16)==0x63 and int(head[4])==int(vec==128)
        rip=int(head[7],16)+(2 if vec==128 else 0)
        rax=int(re.search(r'\bRAX=([0-9a-fA-F]+)',chunk)[1],16)
        flags=int(re.search(r'\bRFL=([0-9a-fA-F]+)',chunk)[1],16)
        real.append((root,vec,rip,int(head[10],16),int(head[3],16),rax,flags))
    assert real==[(e['root'],e['vector'],e['rip'],e['sp'],e['error'],e['rax'],e['flags']) for e in events]
    return len(real),counts

def run(out,prepare_case=prepare,verify_case=verify,before_release=None):
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    disk,cat,pcs=prepare_case(out);before=digest(disk)
    with (out/'compile.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
            '--desktop-owned-shell-865','--native-catalog-sha256',cat,'--output',str(out/'build')],
            cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    iso=out/'build/x86_64/StyleOS-owned-shell-865.iso'
    vars_path=out/'OVMF_VARS.fd';shutil.copyfile(firmware_path('OVMF_VARS','OVMF_VARS'),vars_path)
    q=shutil.which('qemu-system-x86_64');version=subprocess.check_output([q,'--version'],text=True).splitlines()[0]
    assert version.startswith('QEMU emulator version 8.2.2 ')
    serial,trace=out/'serial.log',out/'cpu-trace.log'
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M',
         '-rtc','base=2026-11-25T12:34:00,clock=vm','-display','none','-vga','std',
         '-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S',
         '-drive',f'if=pflash,format=raw,readonly=on,file={firmware_path("OVMF_CODE","OVMF_CODE")}',
         '-drive',f'if=pflash,format=raw,file={vars_path}','-boot','order=d,strict=on','-cdrom',str(iso),
         '-device',f'piix3-ide,id={CONTROLLER},bus=pcie.0,addr=0x5',
         '-drive',f'if=none,id=data,format=raw,snapshot=on,file={disk}',
         '-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive=data,serial=STYLEOS-S864-DATA',
         '-d','int,guest_errors','-D',str(trace)]
    with (out/'qemu.log').open('wb') as err,subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err) as proc:
        try:
            desktop=Desktop(proc,serial,out);assert 'QMP' in desktop.receive()
            desktop.call('qmp_capabilities');desktop.call('cont')
            r=Run(desktop,serial,out);r.line(b'STYLEOS DESKTOP READY')
            desktop.screen('before')
            r.mark();desktop.key('f9')
            r.receipt(b'STYLEOS WIDE75 ACCOUNT',('first','second','peer','flags'),
                      first=7*P,second=7*P,peer=3*P,flags=3)
            r.receipt(b'STYLEOS NATIVE CATALOG',('valid','requested','accepted','resident'),valid=1,requested=3,accepted=3,resident=3)
            apps=admissions(r.lines())
            def all_preempted(lines):
                ev=[v for _,v in records(lines,EVENT,KEYS)]
                return all(len(wait_slices(ev,a,s))>=MIN_SLICES for a,s in zip(apps,pcs))
            r.until(all_preempted,'two LAPIC preemptions in each private workload')
            if before_release is not None:before_release(desktop,r,apps)
            desktop.key('f9')
            r.receipt(b'STYLEOS WIDE76 STOP',('pid','gen','root','peer','legacy','resident','bytes','wire','queue'),resident=2,bytes=10*P,wire=64,queue=1)
            r.receipt(b'STYLEOS NATIVE EMPTY',('used','dispatches'),used=0)
            desktop.screen('after');desktop.call('stop')
            stats=next(item['stats'] for item in desktop.call('query-blockstats')
                       if item.get('qdev','').endswith(DEVICE) or item.get('device')=='data')
            assert stats['wr_operations']==0 and stats['wr_bytes']==0
            desktop.call('quit');assert proc.wait(timeout=15)==0
        finally:
            if proc.poll() is None:proc.kill();proc.wait(timeout=15)
    lines=complete_lines(serial.read_bytes())
    count,slices=verify_case(trace,lines,apps,pcs)
    assert digest(disk)==before
    result=dict(status='PASS',qemu=version,verified_cpl3_events=count,expanded_apps=2,
                legacy_apps=1,pre_release_loop_irqs=slices,leased_app_pages=[7,7,3],
                forced_stop=1,cross_page_ipc=1,out_of_range_refusal=1,guest_disk_writes=0,residual_leased_app_pages=0,iso_sha256=digest(iso),
                data_disk_sha256=before,trace_sha256=digest(trace),uart_sha256=digest(serial))
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    run(parser.parse_args().output)
