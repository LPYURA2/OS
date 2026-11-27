#!/usr/bin/env python3
"""Real shell note store, disposable guarded PIO overlay, two distinct boots."""
from uart_receipts import complete_lines
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

def verify_cold(lines,prefix,data,site,ref,folder,tasks,*,repeatable=False,live_preview=False,mixed=False,round_count=None,interleaved=False):
    if interleaved and not (tasks and mixed and not repeatable and not live_preview):raise ValueError('unsupported interleave profile')
    ok,obs=cold_prior(prefix,33,site['rva'],ref,cold=True)
    if not ok:raise ValueError('cold root '+str(obs))
    root=obs['active']['root']
    if tasks:
        count=(4 if repeatable else 2) if round_count is None else round_count
        if count!=(4 if repeatable else 2) and not (repeatable and live_preview and mixed and count==32):raise ValueError('unsupported workload size')
        n=12 if mixed else 10
        polls=n+1
        begins=[x for x in lines if x.startswith(b'STYLEOS S863L BEGIN ')]
        ends=[x for x in lines if x.startswith(b'STYLEOS S863L DONE ')]
        req=[x for x in lines if x.startswith(b'STYLEOS S863B REQUEST ')]
        if len(begins)!=count or len(ends)!=count or len(req)!=count+int(not repeatable):raise ValueError('cold distinct launch count')
        for i,x in enumerate(req):
            want=f'STYLEOS S863B REQUEST root=0x{root:016x} completed=0x{i:016x} dispatches=0x{n*i:016x} accepted=0x{int(i<count):016x}'.encode()
            if x!=want:raise ValueError('cold request identity or refusal')
        prev=lines.index(b'STYLEOS DESKTOP READY')
        for i in range(count):
            if not prev<lines.index(req[i])<lines.index(begins[i])<lines.index(ends[i]):raise ValueError('cold request order')
            prev=lines.index(ends[i])
        if not repeatable and not prev<lines.index(req[-1]):raise ValueError('cold refusal order')
        frame=sessions=None;workload=[]
        workload_tags=(b'STYLEOS S861 ',b'STYLEOS S861T ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S862',b'STYLEOS S863M ',b'STYLEOS S865 TIMER-CANCEL ',b'STYLEOS S865 TIMER-DRAIN ',b'STYLEOS S865 PAYLOAD ',b'STYLEOS S865 FX-RETIRE ')
        for i,(begin,end) in enumerate(zip(begins,ends)):
            m=re.fullmatch(rb'STYLEOS S863L BEGIN root=0x([0-9a-f]{16}) dispatches=0x([0-9a-f]{16}) frame=0x([0-9a-f]{16}) sessions=0x([0-9a-f]{16})',begin)
            if not m:raise ValueError('cold task begin')
            rr,dispatch,fr,se=map(lambda x:int(x,16),m.groups())
            if rr!=root or dispatch!=n*i:raise ValueError('cold task initial state')
            if interleaved and i==0:
                initial_frame=fr
                m=re.fullmatch(rb'STYLEOS S863L DONE root=0x([0-9a-f]{16}) dispatches=0x([0-9a-f]{16}) frame=0x([0-9a-f]{16}) sessions=0x([0-9a-f]{16})',end)
                if not m:raise ValueError('interleaved task final state')
                er,ed,fr,se=(int(x,16) for x in m.groups())
                if (er,ed)!=(root,n) or fr==initial_frame:raise ValueError('interleaved render')
            elif end!=f'STYLEOS S863L DONE root=0x{root:016x} dispatches=0x{n*(i+1):016x} frame=0x{fr:016x} sessions=0x{se:016x}'.encode():raise ValueError('cold task final state')
            if i and (fr,se)!=(frame,sessions):raise ValueError('cold shell state changed between launches')
            frame,sessions=fr,se
            workload.extend(x for x in lines[lines.index(begin)+1:lines.index(end)] if x.startswith(workload_tags))
        all_workload=[x for x in lines if x.startswith(workload_tags)]
        if workload!=all_workload:raise ValueError('task ran outside admitted launches')
        if not mixed and any(x.startswith(b'STYLEOS S863M ') for x in workload):raise ValueError('unexpected mixed workload in legacy profile')
        from test_desktop_v0 import Frame
        names=['note','after-first-launch','after-tasks']+([f'after-launch-{i}' for i in range(3,count+1)] if repeatable else [])
        frames=[Frame(folder/(name+'.ppm')) for name in names]
        unchanged=frames[1:] if interleaved else frames
        if any((x.w,x.h,x.pixels)!=(unchanged[0].w,unchanged[0].h,unchanged[0].pixels) for x in unchanged[1:]):raise ValueError('QMP screen changed across launch')
        if interleaved:
            from test_desktop_interactive_863 import fnv_text
            def receipt(image):
                if (image.w,image.h)!=(1280,800):raise ValueError('interleave display mode')
                pixels=image.pixels;raw=bytearray(image.w*image.h*4)
                raw[0::4]=pixels[2::3];raw[1::4]=pixels[1::3];raw[2::4]=pixels[0::3]
                return fnv_text(raw)
            if receipt(frames[0])!=initial_frame or receipt(frames[1])!=frame:raise ValueError('actual interleaved framebuffer receipt')
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
        elif len(exits)!=1 or exits[0].split()[-2:]!=[f'dispatches=0x{n*count:016x}'.encode(),b'used=0x0000000000000000']:raise ValueError('exit after refusal')
        ok,taskobs=task_verify(prefix+b'\n'.join(workload)+b'\n',33,site,ref,clean_fnv=frame,cold=True,round_count=count,mixed=mixed)
        if not ok:raise ValueError('cold task hardware '+str(taskobs))
        events=trace_check(data.decode('ascii'),taskobs)
        if len(events)!=1+(n+2)*count:raise ValueError('cold task CPU count')
        slices=[x for x in lines if x.startswith(b'STYLEOS S863P SLICE ')]
        if len(slices)!=polls*count or any(x.startswith((b'STYLEOS S863P WAIT',b'STYLEOS S863P RESUME')) for x in lines):raise ValueError('cold task poll count')
        before=0
        for i,line in enumerate(slices,1):
            m=re.fullmatch(rb'STYLEOS S863P SLICE '+b' '.join(x+b'=0x([0-9a-f]{16})' for x in (b'n',b'before',b'after',b'used',b'processes',b'windows',b'done')),line)
            if not m:raise ValueError('cold task slice')
            seq,old,new,used,procs,wins,done=map(lambda x:int(x,16),m.groups())
            cleanup=i%polls==0
            if (seq,old,new,used,done)!=(i,before,before+int(not cleanup),0 if cleanup else 6,int(cleanup)) or (cleanup and (procs or wins)):raise ValueError('cold task poll binding')
            before=new
        if before!=n*count:raise ValueError('cold task final dispatches')
        io=[x for x in lines if x.startswith(b'STYLEOS S864 ')]
        if interleaved:
            want=[f'STYLEOS S864 {tag} root=0x{root:016x} dispatches=0x0000000000000001{tail}'.encode() for tag,tail in [('HOLD',' used=0x0000000000000006 pid=0x0000000000000001'),('SAVED',' used=0x0000000000000006'),('RESUME','')]]
            if io!=want:raise ValueError('interleaved receipts')
            commits=[x for x in lines if x.startswith(b'STYLEOS S863S COMMIT ')]
            saves=[x for x in lines if x.startswith(b'STYLEOS S863S SAVED ')]
            if len(commits)!=2 or len(saves)!=1 or not lines.index(begins[0])<lines.index(slices[0])<lines.index(io[0])<lines.index(commits[0])<lines.index(commits[1])<lines.index(saves[0])<lines.index(io[1])<lines.index(io[2])<lines.index(slices[1])<lines.index(ends[0]):raise ValueError('interleaved commit order')
        elif io:raise ValueError('unexpected interleaved workload')
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


def verify_four_storage(lines,raw,data,site,ref,folder,write,*,provisioned=False,rc=33,integrated=False):
    """Same four-process trace as live ISO, with an actual save between polls."""
    from runtime_four_864_verify import verify,trace_check,N,order
    from runtime_mixed_863_verify import row
    from test_desktop_v0 import Frame
    from test_desktop_interactive_863 import fnv_text
    def rows(tag):return [x for x in lines if x.startswith(tag+b' ')]
    def expect(line,tag,keys,want):
        if tuple(row(line,tag,keys).values())!=tuple(want):raise ValueError('four storage '+str(tag))
    def frame_hash(frame):
        if (frame.w,frame.h)!=(1280,800):raise ValueError('four storage display mode')
        pixels=frame.pixels;buf=bytearray(frame.w*frame.h*4);buf[0::4]=pixels[2::3];buf[1::4]=pixels[1::3];buf[2::4]=pixels[0::3]
        return fnv_text(buf)
    rounds=8 if integrated else 4
    names=['note','after-first-launch','after-tasks']+[f'after-launch-{i}' for i in range(3,rounds+1)]
    frames=[Frame(folder/(x+'.ppm')) for x in names]
    hashes=[frame_hash(x) for x in frames];final=hashes[1]
    if any(x.pixels!=frames[1].pixels for x in frames[2:]) or (frames[0].pixels==frames[1].pixels)==(write and not integrated):raise ValueError('four storage frame continuity')
    ok,obs=verify(raw,rc,site,ref,cold=True,clean_fnv=final,storage=True,provisioned=provisioned,host_quit=provisioned,round_count=rounds,integrated=integrated)
    if not ok:raise ValueError(obs)
    root=obs['active']['root'];events=trace_check(data.decode('ascii'),obs)
    cold=rows(b'STYLEOS S863B COLD');ready=rows(b'STYLEOS S863 READY')
    if len(cold)!=1 or len(ready)!=1:raise ValueError('four cold boot')
    expect(cold[0],b'STYLEOS S863B COLD',('root','dispatches','arena-next','processes','windows'),(root,0,1,0,0))
    expect(ready[0],b'STYLEOS S863 READY',('root','dispatches','next-pid','next-window','input','x','y','writes'),(root,0,1,1,3,640,600,1024000))
    clocks=rows(b'STYLEOS S863C CLOCK')
    if len(clocks)!=1:raise ValueError('four cold clock')
    expect(clocks[0],b'STYLEOS S863C CLOCK',('root','dispatches','hour','minute','shadow'),(root,0,12,34,0x8d))
    req=rows(b'STYLEOS S863B REQUEST');beg=rows(b'STYLEOS S863L BEGIN');end=rows(b'STYLEOS S863L DONE')
    rb=rows(b'STYLEOS S864 BATCH-BEGIN');re=rows(b'STYLEOS S864 BATCH-END');slices=rows(b'STYLEOS S863P SLICE')
    if len(req)!=rounds+(0 if integrated else 1) or any(len(x)!=rounds for x in (beg,end,rb,re)) or len(slices)!=27*rounds:raise ValueError('four storage poll count')
    tags=(b'STYLEOS S864 ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S861T ',b'STYLEOS S862',b'STYLEOS S863M ')
    workload=[];before=0;session=None;prev=lines.index(ready[0])
    if not lines.index(cold[0])<lines.index(clocks[0])<lines.index(b'STYLEOS DESKTOP READY')<prev:raise ValueError('four cold ordering')
    for i in range(rounds):
        expect(req[i],b'STYLEOS S863B REQUEST',('root','completed','dispatches','accepted'),(root,i,N*i,1))
        begin=row(beg[i],b'STYLEOS S863L BEGIN',('root','dispatches','frame','sessions'))
        done=row(end[i],b'STYLEOS S863L DONE',('root','dispatches','frame','sessions'))
        if tuple(begin.values())!=(root,N*i,hashes[0] if i==0 else final,begin['sessions']) or tuple(done.values())!=(root,N*(i+1),final,done['sessions']):raise ValueError('four storage frame receipts')
        if not (write and i==0 and not integrated) and begin['sessions']!=done['sessions']:raise ValueError('four session changed outside save')
        if session is not None and begin['sessions']!=session:raise ValueError('four session continuity')
        session=done['sessions']
        ids=[lines.index(x) for x in (req[i],beg[i],rb[i],slices[i*27],re[i],slices[i*27+26],end[i])]
        if prev>=ids[0] or ids!=sorted(ids):raise ValueError('four storage launch order')
        prev=ids[-1];workload.extend(x for x in lines[ids[1]+1:ids[-1]] if x.startswith(tags))
        windows=4
        for j,(_,kind) in enumerate(list(order())+[(0,'cleanup')]):
            cleanup=kind=='cleanup'
            if kind=='end':windows-=1
            n=i*27+j+1
            want=(n,before,before+int(not cleanup),0 if cleanup else 3*windows if integrated else 12,0 if cleanup else windows if integrated else 4,0 if cleanup else windows,int(cleanup))
            expect(slices[n-1],b'STYLEOS S863P SLICE',('n','before','after','used','processes','windows','done'),want)
            before=want[2]
    if not integrated:expect(req[-1],b'STYLEOS S863B REQUEST',('root','completed','dispatches','accepted'),(root,4,104,0))
    exits=rows(b'STYLEOS S863 EXIT')
    if provisioned:
        input_events=rows(b'STYLEOS S863 EVENT')
        if exits or len(input_events)<2 or b' key=0x0000000000000058 ' not in input_events[-2] or b' key=0x0000000000000040 ' not in input_events[-1] or b' writes=0x0000000000000000' in input_events[-1] or not max(prev,lines.index(req[-1]))<lines.index(input_events[-2])<lines.index(input_events[-1]):raise ValueError('live post-workload input proof')
    elif len(exits)!=1 or not prev<lines.index(req[-1])<lines.index(exits[0]) or exits[0].split()[-2:]!=[b'dispatches=0x0000000000000068',b'used=0x0000000000000000']:raise ValueError('four storage exit')
    if before!=N*rounds or workload!=[x for x in lines if x.startswith(tags)]:raise ValueError('four storage workload scope')
    io=[rows(b'STYLEOS S864 '+x) for x in (b'HOLD',b'SAVED',b'RESUME')]
    if integrated:
        if any(io):raise ValueError('test hold reached integrated runtime')
        commits=rows(b'STYLEOS S863S COMMIT');docs=rows(b'STYLEOS S863S SAVED')
        if write and (len(commits)!=2 or len(docs)!=1 or not lines.index(ready[0])<lines.index(commits[0])<lines.index(commits[1])<lines.index(docs[0])<lines.index(req[0])):raise ValueError('ordinary pre-workload save order')
    elif write:
        if any(len(x)!=1 for x in io):raise ValueError('four storage interleave count')
        hold,saved,resume=(x[0] for x in io)
        expect(hold,b'STYLEOS S864 HOLD',('root','dispatches','used','pid'),(root,1,12,1))
        expect(saved,b'STYLEOS S864 SAVED',('root','dispatches','used'),(root,1,12))
        expect(resume,b'STYLEOS S864 RESUME',('root','dispatches'),(root,1))
        commits=rows(b'STYLEOS S863S COMMIT');docs=rows(b'STYLEOS S863S SAVED')
        if len(commits)!=2 or len(docs)!=1 or not lines.index(slices[0])<lines.index(hold)<lines.index(commits[0])<lines.index(commits[1])<lines.index(docs[0])<lines.index(saved)<lines.index(resume)<lines.index(slices[1]):raise ValueError('four storage commit order')
    elif any(io):raise ValueError('unexpected reopen mutation')
    return root,events

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--cold',action='store_true');p.add_argument('--tasks',action='store_true');p.add_argument('--build-from',type=Path);p.add_argument('--mixed',action='store_true');p.add_argument('--four',action='store_true');p.add_argument('--iso',action='store_true');p.add_argument('--provisioned',action='store_true');p.add_argument('--integrated',action='store_true');p.add_argument('--owned-shell',action='store_true');p.add_argument('--native-catalog',action='store_true');p.add_argument('--native-timer-load',action='store_true');p.add_argument('--native-bad-rsp',action='store_true');p.add_argument('--native-fault-probe',choices=('divide','execute-nx','single-step','forbidden-int'));p.add_argument('--native-hang',action='store_true');p.add_argument('--native-forced-stop',action='store_true');p.add_argument('--native-shell-boundary',action='store_true');p.add_argument('--native-window-ipc',action='store_true');p.add_argument('--native-window-tile',action='store_true');p.add_argument('--native-window-input',action='store_true');p.add_argument('--native-window-focus',action='store_true');p.add_argument('--native-window-frame',action='store_true');p.add_argument('--extended-desktop',action='store_true');p.add_argument('--process-view',action='store_true');p.add_argument('--process-stop',action='store_true');p.add_argument('--block-service',action='store_true');args=p.parse_args();tasks=args.tasks;cold=args.cold or tasks;mixed=args.mixed
    if args.four and not (tasks and mixed):p.error('--four requires --tasks --mixed')
    if mixed and not tasks:p.error('--mixed requires --tasks')
    if args.iso and not args.four:p.error('--iso requires --four --tasks --mixed')
    if args.iso and args.build_from:p.error('ISO proof requires a fresh linked build')
    if args.provisioned and not args.iso:p.error('--provisioned requires --iso')
    if args.integrated and not args.provisioned:p.error('--integrated requires --provisioned')
    if args.owned_shell and not (args.integrated and args.four):p.error('--owned-shell requires integrated four-profile boot contract')
    if args.native_catalog and not args.owned_shell:p.error('native catalog requires owned shell')
    if args.native_timer_load and not args.native_catalog:p.error('timer load requires external catalog')
    if args.native_bad_rsp and (not args.native_catalog or args.native_timer_load):p.error('malformed RSP requires separate catalog')
    if args.native_fault_probe and (not args.native_catalog or args.native_timer_load or args.native_bad_rsp):p.error('fault probe requires separate catalog')
    if args.native_hang and (not args.native_catalog or args.native_timer_load or args.native_bad_rsp or args.native_fault_probe):p.error('hang requires separate catalog')
    if args.native_forced_stop and (not args.native_catalog or args.native_timer_load or args.native_bad_rsp or args.native_fault_probe or args.native_hang):p.error('forced stop requires separate native catalog')
    if args.native_window_frame and not args.native_window_focus:p.error('frame requires focus fixture')
    if args.native_window_focus and not args.native_window_input:p.error('focus requires window input')
    if args.native_window_input and not args.native_window_tile:p.error('input requires window tile')
    if args.native_window_tile and not args.native_window_ipc:p.error('tile requires window IPC')
    if args.native_window_ipc and not args.native_shell_boundary:p.error('window IPC requires shell boundary')
    if args.native_shell_boundary and not args.native_forced_stop:p.error('shell boundary requires forced-stop profile')
    if args.extended_desktop and (not args.owned_shell or args.native_catalog):p.error('extended desktop requires ordinary no-catalog ISO')
    if args.process_view and not (args.owned_shell and args.native_hang):p.error('process view requires native hang profile')
    if args.process_stop and not args.process_view:p.error('process stop requires live process view')
    if args.block_service and not args.owned_shell:p.error('block service requires owned shell')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build';base=out/'legacy.raw';top=out/'data.qcow2'
    provision(base)
    if args.provisioned:
        with base.open('r+b') as stream:
            stream.seek(4095*512);stream.write(b'STYLEOS-S864-PROVISIONED-V1'.ljust(512,b'\0'))
    catalog_digest=None
    if args.native_catalog:
        from native_catalog_865_verify import provision_catalog
        catalog_digest=provision_catalog(base,timer_load=args.native_timer_load,bad_rsp=args.native_bad_rsp,fault_probe=args.native_fault_probe,hang_load=args.native_hang,forced_stop=args.native_forced_stop,packages=out/'packages' if args.native_shell_boundary else None,window_ipc=args.native_window_ipc,window_tile=args.native_window_tile,window_input=args.native_window_input,window_focus=args.native_window_focus,window_frame=args.native_window_frame)
    q=shutil.which('qemu-system-x86_64');img=shutil.which('qemu-img');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and img and code and vars):raise RuntimeError('real QEMU mandatory')
    if not subprocess.check_output([q,'--version'],text=True).startswith('QEMU emulator version 8.2.2 '):raise RuntimeError('unvalidated fixture')
    def image(*a):return subprocess.check_output([img,*a],stderr=subprocess.STDOUT,timeout=30)
    if args.build_from:
        # Only the repeated cold-storage workload shares a previously built image.
        # The stress orchestrator verifies the immutable linked artifacts for every run.
        if not cold or (tasks and not mixed):raise ValueError('build reuse restricted to cold storage or mixed cold-task stress')
        build=args.build_from.resolve(strict=True)
        (out/'compile.log').write_text('Reusing exact cold image: '+str(build)+'\n')
    else:
        build_args=['--desktop-owned-shell-865' if args.owned_shell else '--desktop-integrated-864'] if args.integrated else ['--test-desktop-cold-tasks-863' if tasks else '--test-desktop-cold-863' if cold else '--test-desktop-storage-service-863',*(['--mixed-cold-tasks-864'] if mixed else []),*(['--four-live-864'] if args.four else []),*(['--provisioned-storage-864'] if args.provisioned else [])]
        if catalog_digest:build_args+=['--native-catalog-sha256',catalog_digest]
        with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',*build_args,'--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    boot=build/'x86_64/styleos.img';boot_sha=sha(boot.read_bytes())
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=None if args.four else bp_site(efi.read_bytes(),mp.read_text()) if cold and not tasks else recycle_site(efi.read_bytes(),mp,mixed=mixed)
    if args.integrated and not args.owned_shell:
        # The integrated hardware-preemptive path has no cooperative fallback.
        # Bind actual linked entry symbols, not the source profile name alone.
        symbols={token for line in mp.read_text().splitlines() for token in line.split()}
        forbidden={'s847_trial','s863_coop_reap','s847_0_start','s847_0_end','s847_0_fault','s847_0_ok','s847_0_deny','s847_0_yield','s847_1_start','s847_1_end','s847_1_exit','s847_1_ok','s847_1_deny','s847_1_yield'}
        required={'s847_enter','s847_dispatch_raw','s848_dispatch_raw','s848_irq_entry','s847_pf_entry','s847_call_entry'}
        forbidden.update('s848_'+label+'_'+suffix for label in 'abcd' for suffix in ('start','yield','loop_end','fault','exit','end'))
        if symbols & forbidden or not required <= symbols:
            raise ValueError('integrated preemptive entry provenance')
        (out/'entry-separation.json').write_text(json.dumps({'required':sorted(required),'absent':sorted(forbidden)},indent=2)+'\n')
    if args.owned_shell:
        # The shell-only image must not require a linked child workload site.
        # Bind the real boot BP and demand absence of the child INT3 label.
        map_text=mp.read_text()
        site={'rva':None}
        forbidden={'s851_bp_return','s851_bp','s851_frame','s851_frame_address','s861_bp_return','s863_live_run','s863_request_tasks','s863_shell_pump',
                   's848_trial','s860_reap','s849_guest_init','s865_fx_guest_test_stale',
                   's865_test_force_timer','s865_test_observe_trap','s865_fixture_source','s865_fixture_observe',
                   's865_fixture_stage','s865_fixture_stage_one','s865_fixture_request_stop','s865_fixture_force_timer'}
        required={'s847_enter','s847_dispatch_raw','s848_dispatch_raw','s848_irq_entry',
                  's865_executor_boot','s865_executor_poll','s865_executor_launch','s865_native_pump','s865_native_launch',
                  's865_fx_guest_open','s865_fx_guest_bind','s865_retire_terminal'}
        forbidden.update('s848_'+label+'_'+suffix for label in 'abcd' for suffix in ('start','yield','loop_end','fault','exit','end'))
        linked={token for line in map_text.splitlines() for token in line.split()}
        present=forbidden.intersection(linked)
        if present or not required<=linked:raise ValueError('native shell link provenance '+str((sorted(present),sorted(required-linked))))
        compiler=json.loads((build/'x86_64/owned-shell-compiler.json').read_text())
        if '-DSTYLEOS_OWNED_SHELL_865=1' not in compiler['flags'] or not compiler['sources'] or set(compiler['sources'])!=set(compiler['source_sha256']):raise ValueError('owned shell compiler manifest')
        excluded={'boot/x86_64/user_preemption_848.c','boot/x86_64/process_trial_847.c','tests/guest/workload_865.c',
                  'kernel/frame_grant_experiment.c','kernel/kernel_mapping_inventory.c',
                  'kernel/kernel_dependency_inventory.c','kernel/descriptor_state_781.c','kernel/desktop_grant_bridge.c',
                  'kernel/kernel_closure_audit.c','boot/x86_64/irq_delivery_807.c'}
        forbidden_paging={'fg_experiment_run','styleos_grant_tables','styleos_grant_users','arch_paging_inventory_inherited','arch_paging_restore',
                          's790_after_int3','s790_on_int3','s790_on_df','s790_guard_fault_ip','s790_int3','s790_isr3','s790_isr_df',
                          'arch_paging_activate','irq807_handler','irq807_terminal','irq842_terminal'}
        if forbidden_paging & linked:raise ValueError('linked historical paging or fault experiment '+str(sorted(forbidden_paging & linked)))
        test_sources={'tests/guest/native_catalog_865.c'} if args.native_catalog else set()
        from native_kernel_profile import SOURCES
        if any(flag.startswith('-DSTYLEOS_TEST_') for flag in compiler['flags']):
            raise ValueError('diagnostic compiler flag in ordinary kernel')
        if compiler['sources']!=list(SOURCES)+sorted(test_sources):
            raise ValueError('ordinary source manifest mismatch')
        if {'s844_guest_run','s849_guest_finish'} & linked:
            raise ValueError('historical arena/FP workload in ordinary kernel')
        expected_profile='native-catalog-acceptance' if args.native_catalog else 'owned-shell-candidate'
        if compiler.get('profile')!=expected_profile or set(compiler.get('test_sources',[]))!=test_sources:
            raise ValueError('native profile identity')
        if ('s865_native_catalog_request' in linked)!=args.native_catalog:raise ValueError('catalog entry leaked or missing')
        if (build/'x86_64/native_catalog_digest.h').exists()!=args.native_catalog:raise ValueError('catalog digest leaked or missing')
        if '-DSTYLEOS_KERNEL_CAPACITY_32=1' not in compiler['flags'] or '-DSTYLEOS_NATIVE_EXECUTOR_OBSERVERS=1' in compiler['flags']:raise ValueError('native capacity/observer flag')
        if ('-DSTYLEOS_NATIVE_PINNED_CATALOG=1' in compiler['flags'])!=args.native_catalog:raise ValueError('catalog policy flag')
        if not {'boot/x86_64/native_executor_865.c','arch/x86_64/boot_failstop.S'}<=set(compiler['sources']):raise ValueError('native executor/failstop source')
        if excluded.intersection(compiler['sources']) or {s for s in compiler['sources'] if s.startswith('tests/')}!=test_sources:
            raise ValueError('owned shell linked diagnostic translation units')
        if any((build/'x86_64').glob('external-workload-*')) or (build/'x86_64/external_workload_865.h').exists():
            raise ValueError('owned shell generated fixture payload')
        for rel,digest in compiler['source_sha256'].items():
            if sha((ROOT/rel).read_bytes())!=digest:raise ValueError('owned shell source mismatch '+rel)
        (out/'entry-separation.json').write_text(json.dumps({'absent':sorted(forbidden),'excluded_sources':sorted(excluded),'external_fixture_generated':False,'profile':expected_profile,'test_sources':sorted(test_sources)},indent=2)+'\n')
    elif args.four:
        from runtime_mixed_863_verify import loop_site
        site=loop_site(efi.read_bytes(),mp,four=True,external=args.integrated)
    external_receipts=[]
    if args.integrated and not args.owned_shell:
        payloads=site.pop('external_payloads')
        if len(payloads)!=4:raise ValueError('external ELF count')
        with base.open('r+b') as stream:
            for i,code_bytes in enumerate(payloads):
                elf=bytearray(4096+len(code_bytes))
                elf[:7]=b'\x7fELF\x02\x01\x01'
                struct.pack_into('<HHIQQQIHHHHHH',elf,16,2,62,1,0x10000000,64,0,0,64,56,1,0,0,0)
                struct.pack_into('<IIQQQQQQ',elf,64,1,5,4096,0x10000000,0x10000000,len(code_bytes),4096,4096)
                elf[4096:]=code_bytes
                lba=3072+16*i;sectors=(len(elf)+511)//512
                stream.seek(lba*512);stream.write(elf.ljust(8192,b'\0'))
                (out/f'external-{i}.elf').write_bytes(elf)
                external_receipts.append((i,lba,len(elf),sectors))
        (out/'external-elf.json').write_text(json.dumps(external_receipts,indent=2))
    if args.extended_desktop:
        # The peer payload is not linked into EFI. Provision its canonical
        # package independently in the bounded legacy data-disk region.
        package=(build/'x86_64/Native-peer.tea').read_bytes()
        if not 22<=len(package)<=16384:raise ValueError('peer package extent')
        header=bytearray(512)
        header[:8]=b'STEA769\0'
        struct.pack_into('<II',header,8,1,len(package))
        header[16:48]=hashlib.sha256(package).digest()
        with base.open('r+b') as stream:
            stream.seek(3000*512)
            stream.write(header)
            stream.write(package.ljust((len(package)+511)//512*512,b'\0'))
        if package in efi.read_bytes():raise ValueError('peer package leaked into EFI')
        (out/'Native-peer.tea').write_bytes(package)
    original=base.read_bytes();base.chmod(0o444)
    image('create','-f','qcow2','-F','raw','-b',str(base),str(top))
    media=None;iso=None;iso_sha=None
    if args.iso:
        from test_desktop_autonomous_863 import verify_preview_media
        iso=build/'x86_64'/('StyleOS-owned-shell-865.iso' if args.owned_shell else 'StyleOS-integrated-864.iso') if args.integrated else out/'StyleOS-owned-root-storage-test.iso'
        if not args.integrated:subprocess.run([sys.executable,str(ROOT/'tools/make_desktop_iso.py'),'--efi',str(efi),'--output',str(iso)],check=True)
        iso_sha=sha(iso.read_bytes());media=verify_preview_media(iso,efi,out)
    ref=reference(out/'host-reference','gcc');assert ref==reference(out/'host-reference-san','clang',True)
    def run(name,write,foreign=False):
        folder=out/name;folder.mkdir();serial=folder/'serial.log';shutil.copyfile(vars,folder/'OVMF_VARS.fd')
        cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-rtc','base=2026-11-25T12:34:00,clock=vm','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={folder/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={boot}','-device','virtio-blk-pci,drive=styleos,bootindex=1','-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
        if args.iso:
            # Optical medium is the only bootable image; no raw virtio boot fallback.
            for item in (f'if=none,id=styleos,format=raw,readonly=on,file={boot}','virtio-blk-pci,drive=styleos,bootindex=1'):
                i=cmd.index(item);del cmd[i-1:i+1]
            cmd+=['-boot','order=d,strict=on','-cdrom',str(iso)]
        if args.provisioned:
            i=cmd.index('isa-debug-exit,iobase=0xf4,iosize=0x04');del cmd[i-1:i+1]
        for node in ({'driver':'file','filename':str(base),'node-name':FILE,'read-only':True},{'driver':'raw','file':FILE,'node-name':RAW,'read-only':True},{'driver':'file','filename':str(top),'node-name':TOPFILE,'read-only':False},{'driver':'qcow2','file':TOPFILE,'backing':RAW,'node-name':TOP,'read-only':False}):cmd+=['-blockdev',json.dumps(node)]
        cmd+=['-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={TOP}'+(',serial='+('FOREIGN-S864-DATA' if foreign else 'STYLEOS-S864-DATA') if args.provisioned else '')]
        trace=folder/'cpu-trace.log';cmd+=['-d','int,guest_errors','-D',str(trace)]
        (folder/'command.json').write_text(json.dumps(cmd))
        with (folder/'qemu.log').open('wb') as err:
            proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err);d=Desktop(proc,serial,folder)
            try:
                def snapshot(tag,pre=False):
                    o={}
                    for key,method,kw in [('status','query-status',{}),('pci','query-pci',{}),('blocks','query-block',{}),('nodes','query-named-block-nodes',{}),('graph','x-debug-query-block-graph',{}),('qtree','human-monitor-command',{'command-line':'info qtree'}),('mtree','human-monitor-command',{'command-line':'info mtree -f -o'}),('unit','qom-get',{'path':'/machine/peripheral/'+DEVICE,'property':'unit'}),('stats','query-blockstats',{'query-nodes':False})]:
                        o[key]=d.call(method,**kw)
                    if args.owned_shell and tag=='gate':o['registers']=d.call('human-monitor-command',**{'command-line':'info registers'})
                    (folder/(tag+'.json')).write_text(json.dumps(o,indent=2))
                    if pre:scope_fixture(o,base,top);verify_legacy_io(o['mtree'])
                    if args.iso:
                        optical=[b for b in o['blocks'] if (b.get('inserted') or {}).get('file')==str(iso)]
                        if len(optical)!=1 or optical[0].get('removable') is not True or optical[0]['inserted'].get('ro') is not True:raise ValueError('missing unique read-only optical boot attachment')
                        if any((b.get('inserted') or {}).get('file')==str(boot) for b in o['blocks']):raise ValueError('unexpected raw boot fallback')
                    return o
                if 'QMP' not in d.receive():raise RuntimeError('QMP greeting')
                d.call('qmp_capabilities');snapshot('prelaunch',True);d.call('cont')
                if args.provisioned:
                    if foreign:
                        wait(proc,serial,b'STYLEOS S850 FAIL 864-provisioned-serial')
                        d.call('stop');denied=snapshot('denied',True)
                        verify_ready(denied);verify_bar(denied['pci'],denied['mtree'],0x6060)
                        if denied['status'].get('status')!='paused' or proc.poll() is not None:raise ValueError('foreign identity did not fail-stop')
                        d.call('quit')
                        if proc.wait(timeout=30)!=0:raise ValueError('foreign host quit')
                        raw=serial.read_bytes()
                        if [x for x in raw.splitlines() if b' FAIL ' in x]!=[b'STYLEOS S850 FAIL 864-provisioned-serial'] or b'STYLEOS S863S ADMIT ' in raw or b'STYLEOS DESKTOP READY' in raw or b'STYLEOS S863S COMMIT ' in raw or b'STYLEOS S863S WAIT' in raw:raise ValueError('foreign identity crossed admission')
                        return {'refusal':'864-provisioned-serial','uart_sha256':sha(raw)}
                    wait(proc,serial,b'STYLEOS S863 READY ')
                    d.call('stop');gate=snapshot('gate',True);verify_ready(gate);verify_bar(gate['pci'],gate['mtree'],0x6060);d.call('cont')
                else:
                    wait(proc,serial,b'STYLEOS S863S WAIT');d.call('stop')
                    gate=snapshot('gate',True);verify_ready(gate);verify_bar(gate['pci'],gate['mtree'],0x6060 if args.iso else 0x60e0);d.call('cont')
                    d.key('b');wait(proc,serial,b'STYLEOS S863 READY ')
                d.wait_uart('DATA MOUNT');d.screen('initial');d.x,d.y=640,600
                if write:
                    d.key('f5');d.key('ctrl-n');d.key('a')
                    if not mixed or args.integrated:d.key('ctrl-s');d.wait_uart('DOCUMENT SAVE')
                else:
                    # Legacy root rows: Documents, Projects, seeded slot 0, saved slot 1.
                    d.key('f2');d.click(688*1280//1536+115,746*128//804+42+12+3*45+12);d.wait_uart('DOCUMENT OPEN')
                if args.integrated:
                    # DOCUMENT SAVE/OPEN is emitted before framebuffer repaint.
                    # Bind the screenshot to the subsequent real input/render
                    # receipt rather than assuming UART completion means paint.
                    marker=b'STYLEOS DESKTOP DOCUMENT '+(b'SAVE' if write else b'OPEN')
                    before=complete_lines(serial.read_bytes())
                    if before.count(marker)!=1:raise ValueError('document marker count')
                    offset=before.index(marker)
                    tag=b'STYLEOS S863 EVENT '
                    count=sum(x.startswith(tag) for x in before[:offset])
                    wait(proc,serial,tag,count+1)
                    receipts=[x for x in complete_lines(serial.read_bytes())[offset+1:] if x.startswith(tag)]
                    if not receipts:raise ValueError('missing post-document render receipt')
                    fields=dict(part.split(b'=',1) for part in receipts[0].split()[3:])
                    # The save scenario uses Notes; reopening a file via
                    # Explorer starts the independent text editor instead.
                    # The Notes observer is not the editor's text buffer.
                    if write:
                        if (int(fields[b'active'],16)!=4 or int(fields[b'text'],16)!=1 or
                            int(fields[b'text-hash'],16)!=0xaf63dc4c8601ec8c or int(fields[b'text-dirty'],16)!=0):
                            raise ValueError('post-save Notes state')
                    else:
                        if int(fields[b'active'],16)!=1 or int(fields[b'note-session'],16)!=0:
                            raise ValueError('post-open editor state')
                        opened=[x for x in complete_lines(serial.read_bytes())[offset+1:] if x.startswith(b'STYLEOS S863S OPENED ')]
                        if len(opened)!=1:raise ValueError('post-open disk evidence')
                        from runtime_mixed_863_verify import row
                        receipt=row(opened[0],b'STYLEOS S863S OPENED',('root','slot','bytes','hash'))
                        if tuple(receipt.values())!=(int(fields[b'root'],16),1,1,0xaf63dc4c8601ec8c):
                            raise ValueError('post-open file content')
                    if not int(fields[b'writes'],16):raise ValueError('post-document render did not write pixels')
                d.screen('note')
                if args.owned_shell:
                    tag=b'STYLEOS S863 EVENT '
                    count=sum(x.startswith(tag) for x in complete_lines(serial.read_bytes()))
                    d.key('f9');wait(proc,serial,tag,count+1)
                    if args.native_catalog:
                        from native_catalog_865_verify import wait_native_empty,wait_hang_progress
                        if args.native_forced_stop:
                            from native_forced_stop_verify import wait_stop_ready,wait_restart_ready
                            if args.native_window_focus:
                                from native_forced_stop_verify import wait_focus_sequence
                                wait_focus_sequence(proc,serial,2)
                                old_x,old_y=d.x,d.y
                                d.move(1,1);d.button(True)
                                wait_focus_sequence(proc,serial,3)
                                d.button(False)
                                wait_focus_sequence(proc,serial,4)
                                d.key('a');wait_focus_sequence(proc,serial,5)
                                # One further real key stays queued for
                                # the existing stop/revocation scrub proof.
                                d.key('b')
                                d.move(old_x,old_y)
                            wait_stop_ready(proc,serial)
                            if args.native_shell_boundary:
                                from native_forced_stop_verify import wait_shell_boundary
                                d.screen('native-before-shell-refresh')
                                d.key('f10');wait_shell_boundary(proc,serial)
                                d.screen('native-live-shell-refresh')
                            d.key('f9');wait_restart_ready(proc,serial)
                            d.key('f9');wait(proc,serial,b'STYLEOS NATIVE STOP-DONE ',1)
                        elif args.native_hang:wait_hang_progress(proc,serial)
                        else:wait_native_empty(proc,serial,30 if args.native_fault_probe else 600 if args.native_timer_load else 180)
                    d.screen('after-denied-fixture')
                    if args.process_view:
                        from native_process_view_02 import exercise as process_view
                        process_view(d,proc,serial,folder,wait)
                        if args.process_stop:
                            from native_process_stop_03 import exercise as process_stop
                            process_stop(d,proc,serial,folder,wait)
                    if args.extended_desktop:
                        from native_desktop_acceptance import exercise as extended
                        extended(d,proc,serial,folder,wait)
                elif tasks:
                    d.key('f9')
                    if mixed and write and not args.integrated:
                        wait(proc,serial,b'STYLEOS S864 HOLD ')
                        d.key('ctrl-s');d.wait_uart('DOCUMENT SAVE')
                        wait(proc,serial,b'STYLEOS S864 RESUME ')
                    wait(proc,serial,b'STYLEOS S863L DONE ',1)
                    d.screen('after-first-launch')
                    d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',2)
                    d.screen('after-tasks')
                    if args.four:
                        for i in range(3,9 if args.integrated else 5):
                            d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',i);d.screen(f'after-launch-{i}')
                    if not args.integrated:d.key('f9');wait(proc,serial,b'STYLEOS S863B REQUEST ',5 if args.four else 3)
                d.call('stop');final=snapshot('final')
                verify_ready(final);verify_bar(final['pci'],final['mtree'],0x6060 if args.iso else 0x60e0)
                stats=next(r['stats'] for r in final['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                if stats['wr_operations']!=(2 if write else 0) or stats['wr_bytes']!=(1024 if write else 0) or (write and stats['flush_operations']<2) or (not write and stats['flush_operations']):raise ValueError('frontend mutation count')
                if any(stats.get(key)!=0 for key in ('unmap_operations','unmap_bytes','failed_rd_operations','invalid_rd_operations','failed_wr_operations','invalid_wr_operations','failed_flush_operations','invalid_flush_operations')):raise ValueError('frontend errors')
                # Independently verify attachment graph after actual write counters
                # have been validated; preserve original unprojected QMP snapshot.
                projection=copy.deepcopy(final)
                ps=next(r['stats'] for r in projection['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                for key in ('wr_operations','wr_bytes','flush_operations'):ps[key]=0
                scope_fixture(projection,base,top);verify_legacy_io(final['mtree'])
                d.call('cont')
                if args.provisioned:
                    tag=b'STYLEOS S863 EVENT '
                    def event_count():return sum(x.startswith(tag) for x in complete_lines(serial.read_bytes()))
                    n=event_count();d.key('f12');wait(proc,serial,tag,n+1)
                    n=event_count();d.key('f6');wait(proc,serial,tag,n+1)
                    if args.native_hang and not args.process_stop:
                        from native_catalog_865_verify import wait_hang_after_input
                        wait_hang_after_input(proc,serial)
                    d.call('stop');live=snapshot('post-f12')
                    if live['status'].get('status')!='paused' or proc.poll() is not None:raise ValueError('not live after F12')
                    verify_ready(live);verify_bar(live['pci'],live['mtree'],0x6060)
                    now=next(r['stats'] for r in live['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                    if any(now[k]!=stats[k] for k in ('wr_operations','wr_bytes','flush_operations','unmap_operations')):raise ValueError('unexpected post-workload mutation')
                    d.screen('post-f12');d.call('quit')
                else:d.call('human-monitor-command',**{'command-line':'sendkey f12 40'})
                rc=proc.wait(timeout=30)
                if rc!=(0 if args.provisioned else 33):raise ValueError('real guest exit')
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
        lines=raw.splitlines()
        if args.native_hang and not args.process_stop:
            # A paused QEMU can leave a partial UART line. Retain the raw
            # file, but validate only the completed receipt boundary.
            from native_catalog_865_verify import complete_uart_lines
            lines=complete_uart_lines(raw)
            last=max(j for j,x in enumerate(lines) if x.startswith(b'STYLEOS NATIVE EVENT '))
            trailing=lines[last+1:]
            if any(not x.startswith(b'STYLEOS NATIVE IRQ-RETIRE ') for x in trailing) or len(trailing)>1:raise ValueError('unexpected trailing hang evidence')
            lines=lines[:last+1]
        commits=[x for x in lines if x.startswith(b'STYLEOS S863S COMMIT ')]
        if len(commits)!=(2 if write else 0):raise ValueError('commit count')
        if write and [int(x.split()[-1].split(b'=')[1],16) for x in commits]!=[20,19]:raise ValueError('payload then header')
        if args.provisioned:
            if lines.count(b'STYLEOS S863S PROVISIONED-BOOT')!=1 or lines.count(b'STYLEOS S863S PROVISIONED-IDENTITY')!=1 or b'STYLEOS S863S WAIT' in lines:raise ValueError('autonomous provisioned identity proof')
            gate=lines.index(b'STYLEOS S863S PROVISIONED-BOOT')
        else:gate=lines.index(b'STYLEOS S863S WAIT')
        prefix=b'\n'.join(lines[:gate])+b'\n'
        data=trace.read_bytes()
        if args.owned_shell:
            from runtime_mixed_863_verify import row
            ok,obs=cold_prior(prefix,rc,site['rva'],ref,cold=True,host_quit=True,capacity=96,clean_boot=True)
            if not ok:raise ValueError('owned shell root '+str(obs))
            root=obs['active']['root']
            regs=json.loads((folder/'gate.json').read_text())['registers']
            def live_reg(pattern):
                found=re.findall(pattern,regs,re.M)
                if len(found)!=1:raise ValueError('live CPU register '+pattern)
                return int(found[0],16)
            active=obs['active'];runtime=obs['runtime']
            if (live_reg(r'\bCR3=([0-9a-fA-F]+)\b'),
                live_reg(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),
                live_reg(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),
                live_reg(r'^TR\s*=([0-9a-fA-F]+)\b'),
                live_reg(r'^CS\s*=([0-9a-fA-F]+)\b'))!=(root,active['gdt'],active['idt'],active['tr'],runtime['cs']):
                raise ValueError('ordinary live descriptor snapshot')
            if live_reg(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b')&0x200:raise ValueError('ordinary shell IF')
            if args.native_catalog:
                from native_catalog_865_verify import verify_catalog
                if args.native_forced_stop:
                    from native_forced_stop_verify import verify_forced_stop
                    events=verify_forced_stop(lines,data.decode('ascii'),obs,shell_boundary=args.native_shell_boundary,window_ipc=args.native_window_ipc,window_tile=args.native_window_tile,window_input=args.native_window_input,window_focus=args.native_window_focus,window_frame=args.native_window_frame)
                else:
                    events=verify_catalog(lines,data.decode('ascii'),obs,timer_load=args.native_timer_load,bad_rsp=args.native_bad_rsp,fault_probe=args.native_fault_probe,hang_load=args.native_hang,ui_stop=args.process_stop)
                if stats['rd_operations']<4+32*9:raise ValueError('native catalog actual disk reads')
            else:
                events=[]
                if args.extended_desktop:
                    from native_forced_stop_verify import records
                    from irq_trace_accept_859 import HEADER
                    admitted=records(lines,b'STYLEOS NATIVE SHELL-CONTROLLER',('pid','gen','root','epoch'))
                    apps=records(lines,b'STYLEOS TEA69 LAUNCHED',('pid','gen','root','window'))
                    if len(admitted)!=17 or len(apps)!=2:raise ValueError('ordinary controller/app receipts')
                    children={a['root'] for _,a in apps}
                    controller_roots={a['root'] for _,a in admitted}
                    refresh_root=admitted[-1][1]['root']
                    if len(children)!=2 or refresh_root in children or refresh_root==root or controller_roots!=children|{refresh_root}:
                        raise ValueError('ordinary isolated root count')
                    trace_text=data.decode('ascii');headers=list(HEADER.finditer(trace_text))
                    syscalls={child:[] for child in controller_roots}
                    for j,m in enumerate(headers):
                        block=trace_text[m.start():headers[j+1].start() if j+1<len(headers) else len(trace_text)]
                        cr3=re.findall(r'\bCR3=([0-9a-fA-F]+)\b',block)
                        if len(cr3)!=1:raise ValueError('ordinary trace root')
                        child=int(cr3[0],16)
                        if child in controller_roots:
                            vec,err,soft,cpl,cs,rip,pc,ss,sp=map(lambda x:int(x,16),m.groups()[1:])
                            if cpl!=3 or cs!=0x5b or ss!=0x63 or err:raise ValueError('ordinary isolated trap')
                            if vec==128:
                                ax=re.findall(r'\bRAX=([0-9a-fA-F]+)\b',block)
                                if len(ax)!=1 or not soft:raise ValueError('ordinary syscall evidence')
                                syscalls[child].append(int(ax[0],16))
                            elif vec!=0x50:raise ValueError('unexpected ordinary child trap')
                        elif child==root and int(m[2],16)!=0x50:
                            raise ValueError('unexpected safe root interrupt')
                    if syscalls[refresh_root]!=[0x105,0x106,2]:raise ValueError('live refresh raw controller syscall sequence')
                    for _,a in apps:
                        child=a['root']
                        n=sum(c['root']==child for _,c in admitted)
                        prefix=[0x105,0x106,2]*n
                        seq=syscalls[child]
                        if seq[:len(prefix)]!=prefix:raise ValueError('ordinary controller raw syscall sequence')
                        app=seq[len(prefix):]
                        if len(app)<3 or app[0]!=0x103 or app[-1]!=2 or any(x!=0x102 for x in app[1:-1]):
                            raise ValueError('ordinary TEA raw frame/input/exit proof')
                    disk=records(lines,b'STYLEOS TEA69 DISK',('bytes','sectors'))
                    package=(out/'Native-peer.tea').read_bytes()
                    if len(disk)!=1 or tuple(disk[0][1].values())!=(len(package),1+(len(package)+511)//512):
                        raise ValueError('external peer read receipt')
                    if stats['rd_operations']<4+disk[0][1]['sectors']:raise ValueError('external package actual disk reads')
                    (folder/'native-perf-counters.json').write_text(json.dumps({
                        'scope':'QEMU TCG; syscall counts, not CPU utilization or physical timing',
                        'controllers':len(admitted),'app_count':len(apps),
                        'app_syscalls':[{ 'pid':a['pid'],'syscalls':len(syscalls[a['root']])-3*sum(c['root']==a['root'] for _,c in admitted),
                        'input_polls':syscalls[a['root']].count(0x102)} for _,a in apps],
                        'external_package_bytes':len(package),'package_sectors':disk[0][1]['sectors'],
                        'disk_reads':stats['rd_operations'],'disk_writes':stats['wr_operations']},indent=2)+'\n')
                else:
                    for m in re.finditer(r'\bCR3=([0-9a-fA-F]+)\b',data.decode('ascii')):
                        if int(m[1],16)==root:raise ValueError('unsolicited interrupt under ordinary safe root')
            marker=b'STYLEOS S865 SHELL-ONLY fixture-launch=deny'
            direct=[x for x in lines if x.startswith(b'STYLEOS OWNED BOOT ')]
            ready=[x for x in lines if x.startswith(b'STYLEOS S863 READY ')]
            if len(direct)!=1 or len(ready)!=1 or lines.count(marker)!=1 or not lines.index(direct[0])<gate<lines.index(marker)<lines.index(ready[0]):raise ValueError('owned shell phase')
            native=[x for x in lines if x.startswith(b'STYLEOS NATIVE RUNTIME READY ')]
            if len(native)!=1 or tuple(row(native[0],b'STYLEOS NATIVE RUNTIME READY',('slots','pages','root')).values())!=(32,96,root) or not lines.index(native[0])<lines.index(marker):raise ValueError('native runtime boot proof')
            r=row(ready[0],b'STYLEOS S863 READY',('root','dispatches','next-pid','next-window','input','x','y','writes'))
            if (r['root'],r['dispatches'],r['next-pid'],r['next-window'],r['input'])!=(root,0,1,1,3):raise ValueError('owned shell initial state')
            inputs=[x for x in lines if x.startswith(b'STYLEOS S863 EVENT ')]
            keys=(0x58,0x40) if args.extended_desktop or args.process_stop else (0x42,0x58,0x40) if args.process_view else (0x43,0x58,0x40)
            if len(inputs)<len(keys) or any(b' key=0x'+f'{key:016x}'.encode()+b' ' not in x for key,x in zip(keys,inputs[-len(keys):])):raise ValueError('owned shell F9/F12/F6 input proof')
            if args.extended_desktop:
                if sum(b' key=0x0000000000000043 ' in x for x in inputs)!=1:raise ValueError('extended F9 input proof')
                from native_desktop_acceptance import verify as verify_extended
                verify_extended(folder,lines,root)
            if args.process_view:
                from native_process_view_02 import verify
                verify(folder,lines,root,live=True)
            if args.process_stop:
                from native_process_stop_03 import verify as verify_stop
                verify_stop(folder,lines)
            denied=(b'STYLEOS S863B REQUEST ',b'STYLEOS S863L BEGIN ',b'STYLEOS S863P SLICE ',b'STYLEOS S864 BATCH-',b'STYLEOS S863 EXIT ')
            if not args.native_catalog:denied+=(b'STYLEOS S848 ',b'STYLEOS NATIVE ADMIT ',b'STYLEOS NATIVE EVENT ')
            if any(x.startswith(denied) for x in lines):raise ValueError('fixture or unsolicited process in owned shell')
            from test_desktop_v0 import Frame
            def unchanged_frame(before,after):
                a,b=Frame(folder/(before+'.ppm')),Frame(folder/(after+'.ppm'))
                if (a.w,a.h,a.pixels)!=(b.w,b.h,b.pixels):
                    raise ValueError('F9 changed shell frame: '+before+' -> '+after)
            if args.native_shell_boundary:
                # The forced-stop ownership fixture publishes two 1x1 tiles
                # at (0,0), with PID2 on top (0xa00002). F9 admits/removes
                # those windows; this is the sole permitted pixel change.
                # F10 may reset legacy shell geometry; compare F9 intervals
                # separately and require the exact live tile across F10.
                before=Frame(folder/'note.ppm')
                live=Frame(folder/'native-before-shell-refresh.ppm')
                refreshed=Frame(folder/'native-live-shell-refresh.ppm')
                stopped=Frame(folder/'after-denied-fixture.ppm')
                side=2 if args.native_window_tile else 1
                fw,fh=(160,120) if args.native_window_frame else (side,side)
                for y in range(fh):
                    for x in range(fw):
                        if args.native_window_frame:
                            rgb='89abcd' if 20<=x<70 and 30<=y<70 else '234567'
                        else:rgb=f'{0xc10002+((y*2+x)<<8):06x}' if args.native_window_tile else 'b10002' if args.native_window_ipc else 'a00002'
                        live.check(x,y,rgb)
                        refreshed.check(x,y,rgb)
                        a=3*(before.w*y+x);b=3*(stopped.w*y+x)
                        if before.pixels[a:a+3]!=stopped.pixels[b:b+3] or (not args.native_window_frame and before.pixels[a:a+3]==live.pixels[a:a+3]):
                            raise ValueError('native ownership tile did not retire to original background')
                before.expect_changes_in(live,[(0,0,fw,fh)])
                refreshed.expect_changes_in(stopped,[(0,0,fw,fh)])
            else:
                unchanged_frame('note','after-denied-fixture')
        elif args.four:
            root,events=verify_four_storage(lines,raw,data,site,ref,folder,write,provisioned=args.provisioned,rc=rc,integrated=args.integrated)
        elif cold:
            root,events=verify_cold(lines,prefix,data,site,ref,folder,tasks,mixed=mixed,interleaved=mixed and write)
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
        dma=[x for x in lines if x.startswith(b'STYLEOS S863S DMA-')]
        keys=(b'root',b'bar4',b'command-status',b'primary-command',b'primary-status',b'secondary-command',b'secondary-status')
        vals=[]
        if len(dma)!=2:raise ValueError('missing DMA readback')
        for line,tag in zip(dma,(b'BEFORE',b'AFTER')):
            match=re.fullmatch(b'STYLEOS S863S DMA-'+tag+b' '+b' '.join(k+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not match:raise ValueError('DMA receipt format')
            rr,bar,cmd_status,*bm=map(lambda x:int(x,16),match.groups())
            if rr!=root or bar!=(0x6061 if args.iso else 0x60e1):raise ValueError('DMA receipt context')
            if any(v not in ((0,4 if i%2 else 8) if args.iso else (0,)) for i,v in enumerate(bm)):raise ValueError('DMA engine started, active, error or unknown bits')
            vals.append(cmd_status)
        if vals[0] not in ((0x02800001,0x02800003,0x02800005,0x02800007) if args.iso else (0x02800007,)) or vals[1]!=(vals[0]&~4):raise ValueError('DMA command transition')
        bars=[x for x in lines if x.startswith(b'STYLEOS S863S BAR4 ')]
        if bars!=[f'STYLEOS S863S BAR4 root=0x{root:016x} bar4=0x{(0x6061 if args.iso else 0x60e1):016x}'.encode()]:raise ValueError('guest BAR4 pin mismatch')
        admits=[x for x in lines if x.startswith(b'STYLEOS S863S ADMIT ')]
        if len(admits)!=1 or not gate<lines.index(dma[0])<lines.index(dma[1])<lines.index(bars[0])<lines.index(admits[0]):raise ValueError('DMA admission order')
        if args.provisioned and not lines.index(bars[0])<lines.index(b'STYLEOS S863S PROVISIONED-IDENTITY')<lines.index(admits[0]):raise ValueError('identity must precede write admission')
        if admits!=[f'STYLEOS S863S ADMIT root=0x{root:016x} first-write=0x0000000000000001 last-write=0x0000000000000048 dma=0x0000000000000000'.encode()]:raise ValueError('admission evidence')
        if any(not x.startswith(f'STYLEOS S863S COMMIT root=0x{root:016x} '.encode()) for x in commits):raise ValueError('commit root')
        if args.block_service:
            from runtime_mixed_863_verify import row
            def block_rows(tag,keys):
                return [(i,row(x,tag,keys)) for i,x in enumerate(lines) if x.startswith(tag+b' ')]
            admits_block=block_rows(b'STYLEOS BLOCK863 ADMIT',('gen','sectors','first-write','last-write','refusals'))
            reads_block=block_rows(b'STYLEOS BLOCK863 READ',('lba','bytes'))
            writes_block=block_rows(b'STYLEOS BLOCK863 WRITE',('lba','bytes'))
            if len(admits_block)!=1 or tuple(admits_block[0][1].values())!=(1,4096,1,72,4):raise ValueError('block scoped admission')
            if len(reads_block)!=1 or tuple(reads_block[0][1].values())!=(0,512):raise ValueError('block real initial read')
            if not lines.index(b'STYLEOS S863S PROVISIONED-IDENTITY')<admits_block[0][0]<lines.index(admits[0])<reads_block[0][0]:raise ValueError('block admission order')
            if [tuple(v.values()) for _,v in writes_block]!=([(20,512),(19,512)] if write else []):raise ValueError('block committed sectors')
            if any(i+1!=lines.index(c) for (i,_),c in zip(writes_block,commits)):raise ValueError('block receipt not tied to physical commit')
        receipts=[x for x in lines if x.startswith(b'STYLEOS S865 EXTERNAL-ELF ')]
        want=[f'STYLEOS S865 EXTERNAL-ELF id=0x{i:016x} lba=0x{lba:016x} bytes=0x{n:016x} sectors=0x{sectors:016x}'.encode() for i,lba,n,sectors in external_receipts]*8
        if receipts!=want:raise ValueError('external executable receipt count/content')
        if external_receipts and stats['rd_operations']<8*sum(x[3] for x in external_receipts):raise ValueError('external executable actual disk reads')
        if external_receipts:
            from runtime_mixed_863_verify import row
            entries=[x for x in lines if x.startswith(b'STYLEOS S864 TASK ')]
            if len(entries)!=32:raise ValueError('external task admission count')
            indices=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S865 EXTERNAL-ELF ')]
            task_indices=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S864 TASK ')]
            for k in range(32):
                if not indices[k]<task_indices[k] or (k>=4 and indices[k]<=task_indices[k-4]):raise ValueError('external ELF load/admission order')
        return {'uart_sha256':sha(raw),'trace_sha256':sha(data),'cpu_events':events,'write':write,'writes':stats['wr_operations'],'document_hash':h,'interleaved_save':mixed and write and not args.integrated,'host_exit':rc,'live_after_f12':args.provisioned}
    first=run('first',True)
    expected=bytearray(original);payload=b'a'
    header=struct.pack('<8sIIBB2x',b'STYLED01',1,1,0,1);h=2166136261
    for c in header+payload:h=((h^c)*16777619)&0xffffffff
    expected[19*512:20*512]=(header+struct.pack('<I',h)).ljust(512,b'\0');expected[20*512:21*512]=payload.ljust(512,b'\0')
    def check(tag):
        path=out/(tag+'.raw');image('convert','-f','qcow2','-O','raw',str(top),str(path))
        if path.read_bytes()!=expected or base.read_bytes()!=original or sha(boot.read_bytes())!=boot_sha or (args.iso and sha(iso.read_bytes())!=iso_sha):raise ValueError('exact disk contents')
    check('after-write');second=run('second',False);check('after-reboot')
    foreign=run('foreign-identity',False,True) if args.provisioned else None
    if args.provisioned:check('after-foreign-refusal')
    (out/'results.json').write_text(json.dumps(dict(status='PASS',diagnostic_only=True,cold_owned_root=cold,cold_task_fixture=tasks,mixed_task_fixture=mixed,four_task_fixture=args.four,ordinary_desktop_verified=args.extended_desktop,native_timer_load=args.native_timer_load,native_bad_rsp=args.native_bad_rsp,native_fault_probe=args.native_fault_probe,native_hang=args.native_hang,native_forced_stop=args.native_forced_stop,integrated_runtime=args.integrated,owned_shell=args.owned_shell,provisioned_boot=args.provisioned,foreign_identity=foreign,boot_format='iso' if args.iso else 'raw',iso_sha256=iso_sha,iso_media=media,first=first,second=second,base_sha256=sha(original),media_sha256=sha(expected),boot_sha256=boot_sha,efi_sha256=sha(efi.read_bytes()),map_sha256=sha(mp.read_bytes())),indent=2))
    print('S863 shell note PIO PASS: real save, separate boot reopen, exact media, zero reboot writes')
if __name__=='__main__':main()
