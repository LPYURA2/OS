"""Real CPL3 expanded app test, separate explicitly pinned diagnostic ISO."""
import argparse
import json
import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
from collections import Counter
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
    # distinct marker there. Push/pop really uses the second stack page.
    emit(b'\xbe'+struct.pack('<I',DATA+P))
    emit(b'\x48\x81\x3e\x78\x56\x34\x12');fail_if_ne()
    emit(b'\x48\x83\xbe\x00\x10\x00\x00\x00');fail_if_ne()
    emit(b'\x48\xc7\x86\x00\x10\x00\x00\x67\x45\x23\x01')
    emit(b'\x68\x21\x43\x65\x07\x5b\x81\xfb\x21\x43\x65\x07');fail_if_ne()
    # Cross at least one real LAPIC time slice. No synthetic host event.
    emit(b'\xb9'+struct.pack('<I',3000000)+b'\xff\xc9\x75\xfc')
    emit(b'\xb8\x01\x00\x00\x00\xcd\x80')
    yield_pc=CODE+P+len(code)
    emit(b'\x85\xc0');fail_if_ne()
    emit(b'\x48\x81\xbe\x00\x10\x00\x00\x67\x45\x23\x01');fail_if_ne()
    if fault:
        terminal=CODE+P+len(code);emit(b'\x0f\x0b')
    else:
        emit(b'\xb8\x02\x00\x00\x00\xcd\x80');terminal=CODE+P+len(code)
    failure=len(code);emit(b'\x0f\x0b')
    for off in fix:struct.pack_into('<i',code,off,failure-off-4)
    return elf(code,True),yield_pc,terminal

def prepare(out):
    disk=out/'expanded.raw'
    provision(disk)
    good,yg,eg=payload(False);bad,yb,eb=payload(True)
    peer=elf(b'\xb8\x01\0\0\0\xcd\x80\xb9'+struct.pack('<I',3000000)+b'\xff\xc9\x75\xfc\xb8\x02\0\0\0\xcd\x80\x0f\x0b',False)
    catalog=bytearray(2048)
    struct.pack_into('<8sII',catalog,0,b'STYLNAT1',1,3)
    with disk.open('r+b') as f:
        f.seek(4095*512);f.write(b'STYLEOS-S864-PROVISIONED-V1'.ljust(512,b'\0'))
        for i,(name,lba,blob) in enumerate((('wide75',3072,good),('fault75',3120,bad),('peer75',3168,peer))):
            struct.pack_into('<II32s8s',catalog,16+i*48,lba,len(blob),hashlib.sha256(blob).digest(),name.encode())
            f.seek(lba*512);f.write(blob)
            (out/(name+'.elf')).write_bytes(blob)
        f.seek(3000*512);f.write(catalog)
    return disk,hashlib.sha256(catalog).hexdigest(),(yg,eg,yb,eb)

def verify(trace,events,apps,pcs):
    yg,eg,yb,eb=pcs
    wide, fault=apps
    roots=[wide['root'],fault['root']]
    assert len(set(roots))==2
    for app,y,terminal,v,state in ((wide,yg,eg,128,4),(fault,yb,eb,6,5)):
        ev=[e for e in events if e['pid']==app['pid']]
        assert ev and all(e['root']==app['root'] and CODE+P<=e['rip']<CODE+2*P
                          and STACK+P<e['sp']<=STACK+2*P for e in ev)
        assert any(e['vector']==0x50 and e['state']==1 for e in ev), 'no real preemption'
        assert any(e['vector']==128 and e['rax']==1 and e['rip']==y and e['state']==1 for e in ev)
        assert (ev[-1]['vector'],ev[-1]['rip'],ev[-1]['state'])==(v,terminal,state)
        assert all(e['state']==1 for e in ev[:-1])
    peer=[e for e in events if e['pid'] not in {wide['pid'],fault['pid']}]
    assert peer and len({e['root'] for e in peer})==1 and peer[0]['root'] not in roots
    assert peer[-1]['state']==4 and peer[-1]['vector']==128 and peer[-1]['rax']==2
    # Bind serial observations to independent actual QEMU CPL3 trace. QEMU
    # INT software instruction IP precedes saved RIP by two bytes.
    data=trace.read_text();heads=list(HEADER.finditer(data))
    real=[]
    for i,head in enumerate(heads):
        if int(head[5])!=3:continue
        chunk=data[head.start():heads[i+1].start() if i+1<len(heads) else len(data)]
        root=int(re.search(r'\bCR3=([0-9a-fA-F]+)',chunk)[1],16)
        if root not in roots+[peer[0]['root']]:continue
        vec=int(head[2],16)
        rip=int(head[7],16)+(2 if vec==128 and head[4]=='1' else 0)
        real.append((root,vec,rip,int(head[10],16),int(head[3],16)))
    expected=[(e['root'],e['vector'],e['rip'],e['sp'],e['error']) for e in events]
    assert real==expected, 'CPL3 trace differs from UART'
    return len(real)

def run(out):
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    disk,cat,pcs=prepare(out);before=digest(disk)
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
            r.receipt(b'STYLEOS NATIVE EMPTY',('used','dispatches'),used=0)
            desktop.screen('after');desktop.call('stop')
            stats=next(item['stats'] for item in desktop.call('query-blockstats')
                       if item.get('qdev','').endswith(DEVICE) or item.get('device')=='data')
            assert stats['wr_operations']==0 and stats['wr_bytes']==0
            desktop.call('quit');assert proc.wait(timeout=15)==0
        finally:
            if proc.poll() is None:proc.kill();proc.wait(timeout=15)
    lines=complete_lines(serial.read_bytes())
    apps=[v for _,v in records(lines,b'STYLEOS WIDE75 ADMIT',('pid','gen','root','bytes'))]
    assert len(apps)==2 and all(a['bytes']==7*P for a in apps)
    events=[v for _,v in records(lines,EVENT,KEYS)]
    count=verify(trace,events,apps,pcs)
    assert digest(disk)==before
    result=dict(status='PASS',qemu=version,verified_cpl3_events=count,expanded_apps=2,
                legacy_apps=1,guest_disk_writes=0,residual_leased_app_pages=0,
                iso_sha256=digest(iso),trace_sha256=digest(trace),uart_sha256=digest(serial))
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    run(parser.parse_args().output)
