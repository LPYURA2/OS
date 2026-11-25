#!/usr/bin/env python3
"""S863: the existing shell after two genuine S862 rounds on the same CR3.
Actual guest exit33 is a test-only F12 checkpoint, not a production shutdown.
No synthetic trace, storage grant or general native task admission.
"""
import argparse,gzip,hashlib,json,re,shutil,subprocess,sys,time
from pathlib import Path
from runtime_recycle_862_verify import verify as prior,trace_check,recycle_site
from test_desktop_frame_852_host import reference
from test_desktop_v0 import Desktop,Frame
from test import ROOT,firmware_path
from fault_site import _unpack,_range

POLICY=b'STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=no reset=no dma-grant=no production-cr3=deny'
EVENT=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')

def fnv_text(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def site_check(efi,mp):
    site=recycle_site(efi,mp);s=mp.read_text()
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    names={**{n:'boot_x86_64_desktop_preflight_850.o' for n in ('s863_shell_guard','s863_shell_observe')},'desktop_v0_run':'kernel_desktop_shell.o',**{n:'arch_x86_64_desktop_io.o' for n in ('desk_input_init','desk_input_next')},**{n:'kernel_desktop_interactive_io_863.o' for n in ('desk_clock','desk_pointer_clock_init','desk_pointer_clock','desk_pointer_near','desk_reboot','desk_ata_probe','desk_ata_read','desk_ata_write','desk_ata_flush','desk_ata_probe_data2','desk_ata_read_data2','desk_ata_write_data2','desk_ata_flush_data2')}}
    for name,obj in names.items():
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+re.escape(name)+r'\s+([0-9a-fA-F]{16})\s+'+re.escape(obj)+r'\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('S863 linked '+name)
        seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S863 section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S863 code rights')
        _range(efi,ro+rel,1,'S863 code');site[name+'_rva']=rva
    return site

def verify(raw,rc,site,ref):
    try:
        lines=raw.splitlines();starts=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863 ADMIT ')]
        if len(starts)!=1 or rc!=33 or not site.get('s863_shell_observe_rva') or any(b' FAIL ' in x for x in lines):raise ValueError('S863 live run')
        start=starts[0];ok,obs=prior(b'\n'.join(lines[:start])+b'\n',rc,site,ref)
        if not ok:raise ValueError(obs)
        r=obs['rounds'][1]['join_roots']['safe'];tail=lines[start:]
        def row(line,tag,keys):
            m=re.fullmatch(b'STYLEOS S863 '+tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m:raise ValueError('S863 '+str(tag))
            return dict(zip(keys,(int(x,16) for x in m.groups())))
        rec=[i for i,x in enumerate(tail) if x.startswith(b'STYLEOS S863 ')]
        if len(rec)!=12 or rec[0]!=0 or rec[1]!=1 or tail[1]!=POLICY:raise ValueError('S863 records')
        if tuple(row(tail[0],b'ADMIT',('root','lpc','dispatches')).values())!=(r,0x29188086,20):raise ValueError('S863 admit')
        markers=(b'STYLEOS S863 STORAGE DENIED',b'STYLEOS DESKTOP DATA UNAVAILABLE',b'STYLEOS S863 CLOCK DENIED',b'STYLEOS DESKTOP KEYBOARD PASS',b'STYLEOS DESKTOP MOUSE PASS',b'STYLEOS DESKTOP READY')
        ix=[tail.index(x) for x in markers if tail.count(x)==1]
        if len(ix)!=len(markers) or ix!=sorted(ix) or not 1<ix[0] or ix[-1]>=rec[4]:raise ValueError('S863 startup')
        rd=row(tail[rec[4]],b'READY',('root','dispatches','next-pid','next-window','input','x','y','writes'))
        if tuple(rd.values())!=(r,20,5,5,3,640,600,1280*800):raise ValueError('S863 ready')
        events=[row(tail[i],b'EVENT',EVENT) for i in rec[5:-1]]
        if len(events)!=6:raise ValueError('S863 event count')
        for i,(e,expected) in enumerate(zip(events,[(0,0x40,0),(1,0,20),(0,0x3f,0),(0,0x1e,0),(0,0x44,0),(0,0x58,0)])):
            if (e['root'],e['n'],e['batch'],e['mouse'],e['key'],e['dx'],e['dy'],e['buttons'],e['modifiers'],e['x'],e['y'])!=(r,i+1,1,*expected,0,0,0,640 if i==0 else 660,600):raise ValueError('S863 input binding')
            if e['ru']!=1 or e['restarts']!=int(i>=4) or e['text']!=(2 if i>=3 else 0) or e['text-hash']!=fnv_text('ф'.encode() if i>=3 else b'') or e['text-dirty']!=int(i>=3) or e['note-session']!=(3 if i>=2 else 0) or e['active'] not in range(5) or e['writes']>1280*800:raise ValueError('S863 state')
        if not (events[0]['writes']>0 and 0<events[1]['writes']<=340 and events[2]['active']==4 and events[2]['writes']>0 and events[3]['active']==4 and events[3]['writes']>0 and events[4]['writes']==1280*800 and events[5]['writes']==0):raise ValueError('S863 shell effect')
        for marker,idx in [(b'STYLEOS DESKTOP INPUT RU',0),(b'STYLEOS DESKTOP CURSOR DAMAGE PASS',1),(b'STYLEOS DESKTOP SHELL RESTART',4)]:
            if tail.count(marker)!=1 or not rec[4+idx]<tail.index(marker)<rec[5+idx]:raise ValueError('S863 shell marker')
        if tuple(row(tail[rec[-1]],b'EXIT',('root','events','dispatches','used')).values())!=(r,6,20,0) or rec[-1]!=len(tail)-1:raise ValueError('S863 exit')
        obs.update(shell_ready=rd,shell_events=events);return True,obs
    except (ValueError,TypeError,KeyError,IndexError) as e:return False,str(e)

def wait(proc,serial,tag,n=1,seconds=100):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if serial.exists() and sum(x.startswith(tag) for x in serial.read_bytes().splitlines())>=n:return
        if proc.poll() is not None:break
        time.sleep(.03)
    raise RuntimeError('missing '+repr(tag))

def screenshots(out):
    frames=[Frame(out/(n+'.ppm')) for n in ('initial','language','cursor','notes','text','restart')]
    if any((f.w,f.h)!=(1280,800) for f in frames):raise ValueError('S863 display mode')
    if frames[0].pixels==frames[1].pixels or frames[2].pixels==frames[3].pixels or frames[3].pixels==frames[4].pixels:raise ValueError('S863 no actual redraw')
    frames[1].expect_changes_in(frames[2],[(640,600,10,17),(660,600,10,17)])
    return {n:hashlib.sha256((out/(n+'.ppm')).read_bytes()).hexdigest() for n in ('initial','language','cursor','notes','text','restart')}

def refusals(raw,rc,site,ref):
    cases=[(raw,0),(raw.replace(POLICY,b''),rc),(raw.replace(b'STYLEOS S863 STORAGE DENIED',b'STYLEOS S863 STORAGE GRANTED'),rc)]
    for line in raw.splitlines():
        if not line.startswith((b'STYLEOS S863 ADMIT ',b'STYLEOS S863 READY ',b'STYLEOS S863 EVENT ',b'STYLEOS S863 EXIT ')):continue
        for m in re.finditer(rb'=0x([0-9a-f]{16})',line):
            a,z=m.span(1);v=int(line[a:z],16)
            # All fields have strict upper bounds; selected IDs/counts have exact matches.
            cases.append((raw.replace(line,line[:a]+f'{v^(1<<63):016x}'.encode()+line[z:],1),rc))
        cases.extend(((raw.replace(line,b'',1),rc),(raw.replace(line,line+b'\n'+line,1),rc)))
    for i,(bad,code) in enumerate(cases):
        if verify(bad,code,site,ref)[0]:raise RuntimeError('S863 accepted mutation '+str(i))
    return len(cases)

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-interactive-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=site_check(efi.read_bytes(),mp);ref=reference(out/'host-reference','gcc');assert ref==reference(out/'host-reference-san','clang',True)
    (out/'site.json').write_text(json.dumps(site,indent=2)+'\n');(out/'reference.json').write_text(json.dumps(ref,indent=2)+'\n')
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True);trace=out/'cpu-trace.log'
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos','-d','int,guest_errors','-D',str(trace)]
    (out/'qemu-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
    with (out/'qemu.log').open('wb') as f:
        proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=f);d=Desktop(proc,serial,out)
        try:
            d.receive();d.call('qmp_capabilities');wait(proc,serial,b'STYLEOS S863 READY ');d.screen('initial')
            for i,(key,name) in enumerate((('f6','language'),(None,'cursor'),('f5','notes'),('a','text'),('f10','restart')),1):
                if key:d.key(key)
                else:d.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}])
                wait(proc,serial,b'STYLEOS S863 EVENT ',i);d.screen(name)
            # Actual test-only guest exit, not a fake host success.
            d.call('human-monitor-command',**{'command-line':'sendkey f12 40'});rc=proc.wait(timeout=30)
        finally:
            d.ready.close()
            if proc.poll() is None:proc.kill();proc.wait()
            proc.stdin.close();proc.stdout.close()
    raw=serial.read_bytes();ok,obs=verify(raw,rc,site,ref)
    if not ok:raise RuntimeError(obs)
    data=trace.read_bytes();events=trace_check(data.decode('ascii'),obs);assert len(events)==25
    with gzip.open(out/'cpu-trace.log.gz','wb') as f:f.write(data)
    trace.unlink();frames=screenshots(out)
    denied=refusals(raw,rc,site,ref)
    result=dict(status='PASS',stage=863,diagnostic_only=True,ordinary_desktop_verified=False,production_cr3_authorized=False,storage_authorized=False,clock_authorized=False,exit=rc,site=site,reference=ref,cpu_events=events,observations=obs,frames=frames,mutation_refusals=denied,mutation_cases=denied,uart_sha256=hashlib.sha256(raw).hexdigest(),trace_sha256=hashlib.sha256(data).hexdigest(),efi_sha256=hashlib.sha256(efi.read_bytes()).hexdigest(),map_sha256=hashlib.sha256(mp.read_bytes()).hexdigest())
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print('S863 actual integrated shell PASS: 25 CPU events, 6 input batches, 6 QMP frames; refusal cases',denied)
if __name__=='__main__':main()
