#!/usr/bin/env python3
"""Real shell note store, disposable guarded PIO overlay, two distinct boots."""
import argparse,copy,hashlib,json,re,shutil,struct,subprocess,sys
from pathlib import Path
from make_desktop_data import provision
from test_desktop_v0 import Desktop
from test_desktop_interactive_863 import wait,prior,trace_check,recycle_site
from test_desktop_frame_852_host import reference
from desktop_switch_851_site import bp_site
from runtime_arena_858_verify import verify as cold_prior
from runtime_recycle_862_verify import verify as task_verify
from irq_trace_accept_859 import HEADER
from test import ROOT,firmware_path
from block_fixture_856_verify import CONTROLLER,DEVICE,RAW,FILE
from overlay_fixture_856 import TOP,TOPFILE
from guest_fixture_857 import scope_fixture,verify_legacy_io
from legacy_ports_857 import verify_ready
from bmdma_857_verify import verify_bar
def sha(b):return hashlib.sha256(b).hexdigest()
def cold_trace(trace,obs):
    root=obs['active']['root'];heads=list(HEADER.finditer(trace));events=[]
    for i,m in enumerate(heads):
        block=trace[m.start():heads[i+1].start() if i+1<len(heads) else len(trace)]
        def one(p):
            v=re.findall(p,block,re.M)
            if len(v)!=1:raise ValueError('cold CPU field '+p)
            return int(v[0],16)
        if one(r'\bCR3=([0-9a-fA-F]+)\b')!=root and not events:continue
        v=tuple(int(x,16) for x in m.groups()[1:])+(one(r'\bCR3=([0-9a-fA-F]+)\b'),one(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b'),one(r'^RIP=([0-9a-fA-F]+)'),one(r'\bRSP=([0-9a-fA-F]+)\b'),one(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),one(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),one(r'^TR\s*=([0-9a-fA-F]+)\b'))
        events.append((int(m[1]),v))
    bp=obs['bp'];rip=bp['rip']-1;active=obs['active']
    expected=(3,0,1,0,bp['cs'],rip,rip,bp['cs']-8,bp['rsp'],root,bp['flags'],rip,bp['rsp'],active['gdt'],active['idt'],active['tr'])
    if len(events)!=1 or events[0][1]!=expected:raise ValueError('cold actual CPU binding')
    return events

def verify_cold(lines,prefix,data,site,ref,folder,tasks,*,repeatable=False,live_preview=False):
    ok,obs=cold_prior(prefix,33,site['rva'],ref,cold=True)
    if not ok:raise ValueError('cold root '+str(obs))
    root=obs['active']['root']
    if tasks:
        count=4 if repeatable else 2
        begins=[x for x in lines if x.startswith(b'STYLEOS S863L BEGIN ')]
        ends=[x for x in lines if x.startswith(b'STYLEOS S863L DONE ')]
        req=[x for x in lines if x.startswith(b'STYLEOS S863B REQUEST ')]
        if len(begins)!=count or len(ends)!=count or len(req)!=count+int(not repeatable):raise ValueError('cold distinct launch count')
        for i,x in enumerate(req):
            want=f'STYLEOS S863B REQUEST root=0x{root:016x} completed=0x{i:016x} dispatches=0x{10*i:016x} accepted=0x{int(i<count):016x}'.encode()
            if x!=want:raise ValueError('cold request identity or refusal')
        prev=lines.index(b'STYLEOS DESKTOP READY')
        for i in range(count):
            if not prev<lines.index(req[i])<lines.index(begins[i])<lines.index(ends[i]):raise ValueError('cold request order')
            prev=lines.index(ends[i])
        if not repeatable and not prev<lines.index(req[-1]):raise ValueError('cold refusal order')
        frame=sessions=None;workload=[]
        for i,(begin,end) in enumerate(zip(begins,ends)):
            m=re.fullmatch(rb'STYLEOS S863L BEGIN root=0x([0-9a-f]{16}) dispatches=0x([0-9a-f]{16}) frame=0x([0-9a-f]{16}) sessions=0x([0-9a-f]{16})',begin)
            if not m:raise ValueError('cold task begin')
            rr,dispatch,fr,se=map(lambda x:int(x,16),m.groups())
            if rr!=root or dispatch!=10*i or end!=f'STYLEOS S863L DONE root=0x{root:016x} dispatches=0x{10*(i+1):016x} frame=0x{fr:016x} sessions=0x{se:016x}'.encode():raise ValueError('cold task final state')
            if i and (fr,se)!=(frame,sessions):raise ValueError('cold shell state changed between launches')
            frame,sessions=fr,se
            workload.extend(x for x in lines[lines.index(begin)+1:lines.index(end)] if x.startswith((b'STYLEOS S861 ',b'STYLEOS S861T ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S862')))
        all_workload=[x for x in lines if x.startswith((b'STYLEOS S861 ',b'STYLEOS S861T ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S862'))]
        if workload!=all_workload:raise ValueError('task ran outside admitted launches')
        from test_desktop_v0 import Frame
        names=['note','after-first-launch','after-tasks']+([f'after-launch-{i}' for i in range(3,count+1)] if repeatable else [])
        frames=[Frame(folder/(name+'.ppm')) for name in names]
        if any((x.w,x.h,x.pixels)!=(frames[0].w,frames[0].h,frames[0].pixels) for x in frames[1:]):raise ValueError('QMP screen changed across launch')
        if live_preview:
            # Independently bind QMP RGB pixels to the guest's BGR0 frame receipt.
            from test_desktop_interactive_863 import fnv_text
            first=frames[0]
            if (first.w,first.h)!=(1280,800):raise ValueError('preview display mode')
            pixels=first.pixels
            raw=bytearray(first.w*first.h*4)
            raw[0::4]=pixels[2::3];raw[1::4]=pixels[1::3];raw[2::4]=pixels[0::3]
            if fnv_text(raw)!=frame:raise ValueError('preview actual framebuffer receipt mismatch')
        exits=[x for x in lines if x.startswith(b'STYLEOS S863 EXIT ')]
        if live_preview:
            if exits:raise ValueError('preview unexpectedly executed test exit')
        elif len(exits)!=1 or exits[0].split()[-2:]!=[f'dispatches=0x{10*count:016x}'.encode(),b'used=0x0000000000000000']:raise ValueError('exit after refusal')
        ok,taskobs=task_verify(prefix+b'\n'.join(workload)+b'\n',33,site,ref,clean_fnv=frame,cold=True,round_count=count)
        if not ok:raise ValueError('cold task hardware '+str(taskobs))
        events=trace_check(data.decode('ascii'),taskobs)
        if len(events)!=1+12*count:raise ValueError('cold task CPU count')
        slices=[x for x in lines if x.startswith(b'STYLEOS S863P SLICE ')]
        if len(slices)!=11*count or any(x.startswith((b'STYLEOS S863P WAIT',b'STYLEOS S863P RESUME')) for x in lines):raise ValueError('cold task poll count')
        before=0
        for i,line in enumerate(slices,1):
            m=re.fullmatch(rb'STYLEOS S863P SLICE '+b' '.join(x+b'=0x([0-9a-f]{16})' for x in (b'n',b'before',b'after',b'used',b'processes',b'windows',b'done')),line)
            if not m:raise ValueError('cold task slice')
            seq,old,new,used,procs,wins,done=map(lambda x:int(x,16),m.groups())
            cleanup=i%11==0
            if (seq,old,new,used,done)!=(i,before,before+int(not cleanup),0 if cleanup else 6,int(cleanup)) or (cleanup and (procs or wins)):raise ValueError('cold task poll binding')
            before=new
        if before!=10*count:raise ValueError('cold task final dispatches')
    else:
        events=cold_trace(data.decode('ascii'),obs)
    cold_rows=[x for x in lines if x.startswith(b'STYLEOS S863B COLD ')]
    if cold_rows!=[f'STYLEOS S863B COLD root=0x{root:016x} dispatches=0x0000000000000000 arena-next=0x0000000000000001 processes=0x0000000000000000 windows=0x0000000000000000'.encode()]:raise ValueError('cold registry')
    if any(x.startswith((b'STYLEOS S844 LIVE ',b'STYLEOS S861 ROOTS ',b'STYLEOS S848 TICK ')) for x in lines[:lines.index(b'STYLEOS DESKTOP READY')]):raise ValueError('pre-shell trial executed')
    clock=f'STYLEOS S863C CLOCK root=0x{root:016x} dispatches=0x0000000000000000 hour=0x000000000000000c minute=0x0000000000000022 shadow=0x000000000000008d'.encode()
    if [x for x in lines if x.startswith(b'STYLEOS S863C CLOCK ')]!=[clock] or not lines.index(cold_rows[0])<lines.index(clock)<lines.index(b'STYLEOS DESKTOP READY'):raise ValueError('cold clock')
    rd=[x for x in lines if x.startswith(b'STYLEOS S863 READY ')]
    wanted=f'STYLEOS S863 READY root=0x{root:016x} dispatches=0x0000000000000000 next-pid=0x0000000000000001 next-window=0x0000000000000001 input=0x0000000000000003 x=0x0000000000000280 y=0x0000000000000258 writes=0x00000000000fa000'.encode()
    if rd!=[wanted]:raise ValueError('cold ready')
    return root,events

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--cold',action='store_true');p.add_argument('--tasks',action='store_true');p.add_argument('--build-from',type=Path);args=p.parse_args();tasks=args.tasks;cold=args.cold or tasks;out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build';base=out/'legacy.raw';top=out/'data.qcow2'
    provision(base);original=base.read_bytes();base.chmod(0o444)
    q=shutil.which('qemu-system-x86_64');img=shutil.which('qemu-img');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and img and code and vars):raise RuntimeError('real QEMU mandatory')
    if not subprocess.check_output([q,'--version'],text=True).startswith('QEMU emulator version 8.2.2 '):raise RuntimeError('unvalidated fixture')
    def image(*a):return subprocess.check_output([img,*a],stderr=subprocess.STDOUT,timeout=30)
    image('create','-f','qcow2','-F','raw','-b',str(base),str(top))
    if args.build_from:
        # Only the repeated cold-storage workload shares a previously built image.
        # The stress orchestrator verifies the immutable linked artifacts for every run.
        if not cold or tasks:raise ValueError('build reuse restricted to cold storage stress')
        build=args.build_from.resolve(strict=True)
        (out/'compile.log').write_text('Reusing exact cold image: '+str(build)+'\n')
    else:
        with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-cold-tasks-863' if tasks else '--test-desktop-cold-863' if cold else '--test-desktop-storage-service-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    boot=build/'x86_64/styleos.img';boot_sha=sha(boot.read_bytes())
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=bp_site(efi.read_bytes(),mp.read_text()) if cold and not tasks else recycle_site(efi.read_bytes(),mp)
    ref=reference(out/'host-reference','gcc');assert ref==reference(out/'host-reference-san','clang',True)
    def run(name,write):
        folder=out/name;folder.mkdir();serial=folder/'serial.log';shutil.copyfile(vars,folder/'OVMF_VARS.fd')
        cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-rtc','base=2026-11-25T12:34:00,clock=vm','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={folder/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={boot}','-device','virtio-blk-pci,drive=styleos,bootindex=1','-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
        for node in ({'driver':'file','filename':str(base),'node-name':FILE,'read-only':True},{'driver':'raw','file':FILE,'node-name':RAW,'read-only':True},{'driver':'file','filename':str(top),'node-name':TOPFILE,'read-only':False},{'driver':'qcow2','file':TOPFILE,'backing':RAW,'node-name':TOP,'read-only':False}):cmd+=['-blockdev',json.dumps(node)]
        cmd+=['-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={TOP}']
        trace=folder/'cpu-trace.log';cmd+=['-d','int,guest_errors','-D',str(trace)]
        (folder/'command.json').write_text(json.dumps(cmd))
        with (folder/'qemu.log').open('wb') as err:
            proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err);d=Desktop(proc,serial,folder)
            try:
                def snapshot(tag,pre=False):
                    o={}
                    for key,method,kw in [('status','query-status',{}),('pci','query-pci',{}),('blocks','query-block',{}),('nodes','query-named-block-nodes',{}),('graph','x-debug-query-block-graph',{}),('qtree','human-monitor-command',{'command-line':'info qtree'}),('mtree','human-monitor-command',{'command-line':'info mtree -f -o'}),('unit','qom-get',{'path':'/machine/peripheral/'+DEVICE,'property':'unit'}),('stats','query-blockstats',{'query-nodes':False})]:
                        o[key]=d.call(method,**kw)
                    (folder/(tag+'.json')).write_text(json.dumps(o,indent=2))
                    if pre:scope_fixture(o,base,top);verify_legacy_io(o['mtree'])
                    return o
                if 'QMP' not in d.receive():raise RuntimeError('QMP greeting')
                d.call('qmp_capabilities');snapshot('prelaunch',True);d.call('cont')
                wait(proc,serial,b'STYLEOS S863S WAIT');d.call('stop')
                gate=snapshot('gate',True);verify_ready(gate);verify_bar(gate['pci'],gate['mtree']);d.call('cont')
                d.key('b');wait(proc,serial,b'STYLEOS S863 READY ');d.wait_uart('DATA MOUNT');d.screen('initial');d.x,d.y=640,600
                if write:
                    d.key('f5');d.key('ctrl-n');d.key('a');d.key('ctrl-s');d.wait_uart('DOCUMENT SAVE')
                else:
                    # Legacy root rows: Documents, Projects, seeded slot 0, saved slot 1.
                    d.key('f2');d.click(688*1280//1536+115,746*128//804+42+12+3*45+12);d.wait_uart('DOCUMENT OPEN')
                d.screen('note')
                if tasks:
                    d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',1)
                    d.screen('after-first-launch')
                    d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',2)
                    d.screen('after-tasks')
                    d.key('f9');wait(proc,serial,b'STYLEOS S863B REQUEST ',3)
                d.call('stop');final=snapshot('final')
                verify_ready(final);verify_bar(final['pci'],final['mtree'])
                stats=next(r['stats'] for r in final['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                if stats['wr_operations']!=(2 if write else 0) or stats['wr_bytes']!=(1024 if write else 0) or (write and stats['flush_operations']<2) or (not write and stats['flush_operations']):raise ValueError('frontend mutation count')
                if any(stats.get(key)!=0 for key in ('unmap_operations','unmap_bytes','failed_rd_operations','invalid_rd_operations','failed_wr_operations','invalid_wr_operations','failed_flush_operations','invalid_flush_operations')):raise ValueError('frontend errors')
                # Independently verify attachment graph after actual write counters
                # have been validated; preserve original unprojected QMP snapshot.
                projection=copy.deepcopy(final)
                ps=next(r['stats'] for r in projection['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                for key in ('wr_operations','wr_bytes','flush_operations'):ps[key]=0
                scope_fixture(projection,base,top);verify_legacy_io(final['mtree'])
                d.call('cont');d.call('human-monitor-command',**{'command-line':'sendkey f12 40'})
                if proc.wait(timeout=30)!=33:raise ValueError('real guest exit')
            except Exception as original_error:
                # Capture the still-live device state before teardown. Never retry
                # a failed save or resume a guest that may have quarantined media.
                try:
                    if proc.poll() is None:
                        d.call('stop')
                        snapshot('failure')
                        d.screen('failure')
                except Exception as capture_error:
                    (folder/'failure-capture-error.txt').write_text(repr(capture_error)+'\n')
                (folder/'failure.txt').write_text(repr(original_error)+'\n')
                raise
            finally:
                d.ready.close()
                if proc.poll() is None:proc.kill();proc.wait()
                proc.stdin.close();proc.stdout.close()
        raw=serial.read_bytes()
        if b' FAIL ' in raw or b'STYLEOS DESKTOP DOCUMENT SAVE ERROR' in raw:raise ValueError('guest failure')
        lines=raw.splitlines();commits=[x for x in lines if x.startswith(b'STYLEOS S863S COMMIT ')]
        if len(commits)!=(2 if write else 0):raise ValueError('commit count')
        if write and [int(x.split()[-1].split(b'=')[1],16) for x in commits]!=[20,19]:raise ValueError('payload then header')
        gate=lines.index(b'STYLEOS S863S WAIT')
        prefix=b'\n'.join(lines[:gate])+b'\n'
        data=trace.read_bytes()
        if cold:
            root,events=verify_cold(lines,prefix,data,site,ref,folder,tasks)
        else:
            ok,obs=prior(prefix,33,site,ref)
            if not ok:raise ValueError('prior lifecycle '+str(obs))
            events=trace_check(data.decode('ascii'),obs)
            if len(events)!=25:raise ValueError('CPU event count')
            root=obs['rounds'][1]['join_roots']['safe']
        h=((14695981039346656037^97)*1099511628211)&((1<<64)-1)
        tag=b'SAVED' if write else b'OPENED'
        wanted=b'STYLEOS S863S '+tag+f' root=0x{root:016x} slot=0x0000000000000001 bytes=0x0000000000000001 hash=0x{h:016x}'.encode()
        docs=[x for x in lines if x.startswith((b'STYLEOS S863S SAVED',b'STYLEOS S863S OPENED'))]
        if docs!=[wanted]:raise ValueError('actual draft content')
        original=b'STYLEOS DESKTOP DOCUMENT SAVE' if write else b'STYLEOS DESKTOP DOCUMENT OPEN'
        if lines.count(original)!=1 or lines.index(original)+1!=lines.index(wanted):raise ValueError('document evidence order')
        admits=[x for x in lines if x.startswith(b'STYLEOS S863S ADMIT ')]
        if admits!=[f'STYLEOS S863S ADMIT root=0x{root:016x} first-write=0x0000000000000001 last-write=0x0000000000000048 dma=0x0000000000000000'.encode()]:raise ValueError('admission evidence')
        if any(not x.startswith(f'STYLEOS S863S COMMIT root=0x{root:016x} '.encode()) for x in commits):raise ValueError('commit root')
        return {'uart_sha256':sha(raw),'trace_sha256':sha(data),'cpu_events':events,'write':write,'writes':stats['wr_operations'],'document_hash':h}
    first=run('first',True)
    expected=bytearray(original);payload=b'a'
    header=struct.pack('<8sIIBB2x',b'STYLED01',1,1,0,1);h=2166136261
    for c in header+payload:h=((h^c)*16777619)&0xffffffff
    expected[19*512:20*512]=(header+struct.pack('<I',h)).ljust(512,b'\0');expected[20*512:21*512]=payload.ljust(512,b'\0')
    def check(tag):
        path=out/(tag+'.raw');image('convert','-f','qcow2','-O','raw',str(top),str(path))
        if path.read_bytes()!=expected or base.read_bytes()!=original or sha(boot.read_bytes())!=boot_sha:raise ValueError('exact disk contents')
    check('after-write');second=run('second',False);check('after-reboot')
    (out/'results.json').write_text(json.dumps(dict(status='PASS',diagnostic_only=True,cold_owned_root=cold,cold_task_fixture=tasks,ordinary_desktop_verified=False,first=first,second=second,base_sha256=sha(original),media_sha256=sha(expected),boot_sha256=boot_sha,efi_sha256=sha(efi.read_bytes()),map_sha256=sha(mp.read_bytes())),indent=2))
    print('S863 shell note PIO PASS: real save, separate boot reopen, exact media, zero reboot writes')
if __name__=='__main__':main()
