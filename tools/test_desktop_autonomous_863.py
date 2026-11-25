#!/usr/bin/env python3
"""Cold boot without external permission/input, storage fail-closed, actual tasks."""
import argparse,hashlib,json,os,shutil,subprocess,sys,time
from pathlib import Path
from test_desktop_v0 import Desktop
from test_desktop_interactive_863 import wait,recycle_site
from test_desktop_storage_service_863 import verify_cold
from test_desktop_frame_852_host import reference
from test import ROOT,firmware_path
def sha(b):return hashlib.sha256(b).hexdigest()
def verify_preview_media(iso,efi,out):
    """Bind the firmware boot extent AND the ordinary ISO copy to the linked EFI."""
    folder=out/'iso-evidence';folder.mkdir()
    report=subprocess.run(['xorriso','-indev',str(iso),'-report_el_torito','plain'],
                          check=True,capture_output=True,text=True).stdout
    (folder/'el-torito.txt').write_text(report)
    rows=[line.split(':',1)[1].split() for line in report.splitlines()
          if line.startswith('El Torito boot img :')]
    if len(rows)!=1 or len(rows[0])!=8 or rows[0][1:4]!=['UEFI','y','none']:
        raise ValueError('preview must have exactly one bootable UEFI no-emulation entry')
    lba=int(rows[0][-1])
    if lba<16:raise ValueError('invalid EFI boot extent')
    esp=folder/'EFI_BOOT.IMG';tree_efi=folder/'tree-BOOTX64.EFI';boot_efi=folder/'boot-BOOTX64.EFI'
    subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),
                    '-extract','/EFI_BOOT.IMG',str(esp),
                    '-extract','/EFI/BOOT/BOOTX64.EFI',str(tree_efi)],check=True)
    font_notice=folder/'DESKTOP-FONT.TXT'
    subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),
                    '-extract','/LICENSES/DESKTOP-FONT.TXT',str(font_notice)],check=True)
    original_notice=Path(__file__).resolve().parents[1]/'docs/licenses/desktop-font.txt'
    if font_notice.read_bytes()!=original_notice.read_bytes():raise ValueError('embedded font license differs')
    image=esp.read_bytes()
    if len(image)!=16*1024*1024:raise ValueError('unexpected EFI partition size')
    with iso.open('rb') as stream:
        stream.seek(lba*2048)
        if stream.read(len(image))!=image:raise ValueError('catalog boot extent differs from ISO ESP')
    subprocess.run(['mcopy','-i',str(esp),'::/EFI/BOOT/BOOTX64.EFI',str(boot_efi)],
                   check=True,env={**os.environ,'MTOOLSRC':os.devnull})
    expected=efi.read_bytes()
    if tree_efi.read_bytes()!=expected or boot_efi.read_bytes()!=expected:
        raise ValueError('packaged EFI differs from linked executable')
    return dict(font_notice_sha256=sha(font_notice.read_bytes()),boot_lba=lba,esp_sha256=sha(image),tree_efi_sha256=sha(tree_efi.read_bytes()),
                boot_efi_sha256=sha(boot_efi.read_bytes()))

def verify_pointer_clock(lines,root):
    """Recompute clock bounds from actual RTC edge receipts, not a PASS string."""
    import re
    def fields(tag,keys):
        rows=[x for x in lines if x.startswith(tag)]
        result=[]
        for row in rows:
            m=re.fullmatch(tag+b' '+b' '.join(k+b'=0x([0-9a-f]{16})' for k in keys),row)
            if not m:raise ValueError('pointer timing receipt')
            result.append(tuple(int(v,16) for v in m.groups()))
        return rows,result
    rows,edges=fields(b'STYLEOS S863T EDGE',[b'root',b'n',b'second',b'lo',b'hi'])
    pr,ptr=fields(b'STYLEOS S863T POINTER',[b'root',b'min-hz',b'max-hz',b'hz',b'stamp'])
    if len(edges)!=3 or len(ptr)!=1:raise ValueError('missing live pointer calibration')
    lo,hi=0,10000000000
    for i,(r,n,sec,start,end) in enumerate(edges):
        if r!=root or n!=i or not 0<=sec<86400 or not 0<start<=end:
            raise ValueError('pointer edge identity')
        if i:
            old=edges[i-1]
            a,b=start-old[4],end-old[3]
            if sec!=(old[2]+1)%86400 or not 0<a<=b<=10000000000 or b-a>a//100:
                raise ValueError('pointer edge interval')
            lo=max(lo,a);hi=min(hi,b)
    if not 0<lo<=hi or ptr[0][:4]!=(root,lo,hi,lo+(hi-lo)//2) or not ptr[0][4]:
        raise ValueError('pointer frequency')
    if not lines.index(rows[0])<lines.index(rows[1])<lines.index(rows[2])<lines.index(pr[0])<lines.index(b'STYLEOS DESKTOP READY'):
        raise ValueError('pointer receipt order')
    return dict(min_hz=lo,max_hz=hi,estimate_hz=ptr[0][3],first_stamp=ptr[0][4])

def refuse_smp_preview(cmd,vars,out,iso,expected_hash):
    """Use the same ISO, fresh firmware variables and actual two-vCPU QEMU."""
    folder=out/'refuse-smp';folder.mkdir()
    serial=folder/'serial.log';trace=folder/'cpu-trace.log'
    varfile=folder/'OVMF_VARS.fd';shutil.copyfile(vars,varfile)
    args=list(cmd)
    args[args.index('-smp')+1]='2,sockets=1,cores=2,threads=1'
    args[args.index('-serial')+1]=f'file:{serial}'
    args[args.index('-D')+1]=str(trace)
    for i,arg in enumerate(args):
        if arg.startswith('if=pflash,format=raw,file='):
            args[i]=f'if=pflash,format=raw,file={varfile}'
    (folder/'command.json').write_text(json.dumps(args,indent=2))
    with (folder/'qemu.log').open('wb') as err:
        proc=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err)
        d=Desktop(proc,serial,folder)
        try:
            if 'QMP' not in d.receive():raise RuntimeError('SMP QMP greeting')
            d.call('qmp_capabilities')
            cpus=d.call('query-cpus-fast')
            if len(cpus)!=2 or {cpu['cpu-index'] for cpu in cpus}!={0,1}:
                raise ValueError('negative fixture not two actual vCPUs')
            wait(proc,serial,b'STYLEOS S790 FAIL 863-platform-cpu')
            d.call('stop')
            status=d.call('query-status')
            stats=d.call('query-blockstats',**{'query-nodes':False})
            if status.get('status')!='paused' or proc.poll() is not None:
                raise ValueError('negative guest did not remain in fail-stop')
            if not stats or any(x['stats']['wr_operations'] or x['stats']['wr_bytes'] for x in stats):
                raise ValueError('negative fixture wrote to media')
            raw=serial.read_bytes();lines=raw.splitlines()
            refusal=b'STYLEOS S790 FAIL 863-platform-cpu'
            scope=b'STYLEOS S790 CPU-SCOPE enabled=0x0000000000000002'
            if [x for x in lines if b' FAIL ' in x]!=[refusal]:
                raise ValueError('negative fixture wrong refusal')
            if [x for x in lines if x.startswith(b'STYLEOS S790 CPU-SCOPE ')]!=[scope] or lines[-2:]!=[scope,refusal]:
                raise ValueError('negative fixture wrong actual MADT CPU count or refusal order')
            if any(x.startswith((b'STYLEOS S850 ROOT ',b'STYLEOS S863 ',b'STYLEOS S863S ',b'STYLEOS S863L ',b'STYLEOS DESKTOP READY')) for x in lines):
                raise ValueError('negative fixture crossed desktop root admission')
            d.call('quit')
            if proc.wait(timeout=30)!=0:raise ValueError('negative host exit')
        finally:
            d.ready.close()
            if proc.poll() is None:proc.kill();proc.wait()
            proc.stdin.close();proc.stdout.close()
    if sha(iso.read_bytes())!=expected_hash:raise ValueError('negative ISO changed')
    result=dict(vcpus=cpus,host_status=status,block_stats=stats,
                refusal='863-platform-cpu',uart_sha256=sha(raw),
                iso_sha256=expected_hash)
    (folder/'results.json').write_text(json.dumps(result,indent=2))
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--preview',action='store_true');p.add_argument('--four',action='store_true');p.add_argument('--rounds',type=int,choices=(4,8,32),default=4);args=p.parse_args();preview=args.preview;rounds=args.rounds
    if args.four and (not preview or rounds not in (4,8)):p.error('four live requires four- or eight-round preview')
    if not args.four and rounds==8:p.error('eight rounds require four live contexts')
    if rounds!=4 and not preview:p.error('extended workload requires the mixed ISO preview')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--desktop-owned-root-preview' if preview else '--test-desktop-autonomous-863',*(['--four-live-864'] if args.four else []),'--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=recycle_site(efi.read_bytes(),mp,mixed=preview) if not args.four else __import__('runtime_mixed_863_verify').loop_site(efi.read_bytes(),mp,four=True)
    for forbidden in ('s863_disk_write','s863_disk_read','storage_commit_863','ata_runtime_write_863','disk_admit863'):
        if forbidden in mp.read_text():raise ValueError('storage backend linked '+forbidden)
    ref=reference(out/'host-reference','gcc');assert ref==reference(out/'host-reference-san','clang',True)
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';trace=out/'cpu-trace.log'
    boot=build/'x86_64/styleos.img';before=sha(boot.read_bytes())
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-rtc','base=2026-11-25T12:34:00,clock=vm','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={boot}','-device','virtio-blk-pci,drive=styleos','-d','int,guest_errors','-D',str(trace)]
    media_evidence=None
    if preview:
        # Boot the packaged ISO, not a substitute raw image. No debug exit device.
        iso=build/'x86_64/StyleOS-owned-root-preview.iso';iso_before=sha(iso.read_bytes())
        media_evidence=verify_preview_media(iso,efi,out)
        for device in ('isa-debug-exit,iobase=0xf4,iosize=0x04','virtio-blk-pci,drive=styleos'):
            i=cmd.index(device);del cmd[i-1:i+1]
        i=cmd.index(f'if=none,id=styleos,format=raw,readonly=on,file={boot}');del cmd[i-1:i+1]
        cmd+=['-boot','order=d,strict=on','-cdrom',str(iso)]
    (out/'command.json').write_text(json.dumps(cmd))
    with (out/'qemu.log').open('wb') as err:
        proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err);d=Desktop(proc,serial,out)
        try:
            if 'QMP' not in d.receive():raise RuntimeError('QMP greeting')
            d.call('qmp_capabilities')
            # No -S, cont, key or device admission handshake before this marker.
            wait(proc,serial,b'STYLEOS S863 READY ')
            d.screen('initial');d.key('f5');d.key('ctrl-n');d.key('a')
            d.screen('note')
            elapsed=[]
            started=time.monotonic_ns()
            d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',1);elapsed.append(time.monotonic_ns()-started);d.screen('after-first-launch')
            started=time.monotonic_ns()
            d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',2);elapsed.append(time.monotonic_ns()-started);d.screen('after-tasks')
            for i in range(3,rounds+1):
                started=time.monotonic_ns()
                d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',i);elapsed.append(time.monotonic_ns()-started);d.screen(f'after-launch-{i}')
            d.call('stop');stats=d.call('query-blockstats',**{'query-nodes':False})
            (out/'stats.json').write_text(json.dumps(stats,indent=2))
            if not stats or any(x['stats']['wr_operations'] or x['stats']['wr_bytes'] for x in stats):raise ValueError('unexpected write')
            d.call('cont')
            if preview:
                # Prove F12 does not halt/exit, then a second actual input repaints UI.
                tag=b'STYLEOS S863 EVENT '
                def event_count():return sum(x.startswith(tag) for x in serial.read_bytes().splitlines())
                n=event_count();d.key('f12');wait(proc,serial,tag,n+1)
                n=event_count();d.key('f6');wait(proc,serial,tag,n+1)
                d.call('stop')
                status=d.call('query-status');(out/'final-status.json').write_text(json.dumps(status))
                if status.get('status')!='paused' or proc.poll() is not None:raise ValueError('preview not live at host stop')
                frame=d.screen('after-f12-language');previous=d.screen('final')
                if frame.pixels!=previous.pixels:raise ValueError('unstable stopped frame')
                from test_desktop_v0 import Frame
                if frame.pixels==Frame(out/f'after-launch-{rounds}.ppm').pixels:raise ValueError('no UI response after F12')
                final_stats=d.call('query-blockstats',**{'query-nodes':False})
                (out/'final-stats.json').write_text(json.dumps(final_stats,indent=2))
                if not final_stats or any(x['stats']['wr_operations'] or x['stats']['wr_bytes'] for x in final_stats):raise ValueError('unexpected final media write')
                d.call('quit');rc=proc.wait(timeout=30)
            else:
                d.call('human-monitor-command',**{'command-line':'sendkey f12 40'});rc=proc.wait(timeout=30)
        finally:
            d.ready.close()
            if proc.poll() is None:proc.kill();proc.wait()
            proc.stdin.close();proc.stdout.close()
    raw=serial.read_bytes();lines=raw.splitlines();data=trace.read_bytes()
    if rc!=(0 if preview else 33) or b' FAIL ' in raw or sha(boot.read_bytes())!=before:raise ValueError('actual exit or boot media')
    if preview:
        if sha(iso.read_bytes())!=iso_before:raise ValueError('ISO changed')
        last=[x for x in lines if x.startswith(b'STYLEOS S863 EVENT ')][-2:]
        if len(last)!=2 or b' key=0x0000000000000058 ' not in last[0] or b' key=0x0000000000000040 ' not in last[1] or b' writes=0x0000000000000000' in last[1]:raise ValueError('post-workload live input binding')
    if lines.count(b'STYLEOS DESKTOP DATA UNAVAILABLE')!=1 or lines.count(b'STYLEOS S863 STORAGE DENIED')<1 or b'STYLEOS DESKTOP DATA MOUNT' in lines or any(x.startswith(b'STYLEOS S863S ') for x in lines):raise ValueError('storage fail-closed')
    policy=b'STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=rtc-service reset=no dma-grant=no production-cr3=deny'
    if lines.count(policy)!=1:raise ValueError('policy')
    cold=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863B COLD '))
    prefix=b'\n'.join(lines[:cold])+b'\n'
    if args.four:
        from runtime_four_864_verify import verify,trace_check,N,order
        from runtime_mixed_863_verify import row
        from test_desktop_v0 import Frame
        from test_desktop_interactive_863 import fnv_text
        names=['note','after-first-launch','after-tasks']+[f'after-launch-{i}' for i in range(3,rounds+1)]
        frames=[Frame(out/(name+'.ppm')) for name in names]
        first=frames[0]
        if (first.w,first.h)!=(1280,800) or any((x.w,x.h,x.pixels)!=(first.w,first.h,first.pixels) for x in frames[1:]):raise ValueError('four live framebuffer preservation')
        pixels=first.pixels;buf=bytearray(first.w*first.h*4);buf[0::4]=pixels[2::3];buf[1::4]=pixels[1::3];buf[2::4]=pixels[0::3]
        fnv=fnv_text(buf)
        ok,obs=verify(raw,33,site,ref,cold=True,clean_fnv=fnv,round_count=rounds)
        if not ok:raise ValueError(obs)
        root=obs['active']['root'];events=trace_check(data.decode('ascii'),obs)
        def rows(tag):return [x for x in lines if x.startswith(tag+b' ')]
        cold_rows=rows(b'STYLEOS S863B COLD')
        if len(cold_rows)!=1 or tuple(row(cold_rows[0],b'STYLEOS S863B COLD',('root','dispatches','arena-next','processes','windows')).values())!=(root,0,1,0,0):raise ValueError('four cold registry')
        req=rows(b'STYLEOS S863B REQUEST');beg=rows(b'STYLEOS S863L BEGIN');end=rows(b'STYLEOS S863L DONE');slices=rows(b'STYLEOS S863P SLICE')
        rb=rows(b'STYLEOS S864 BATCH-BEGIN');re=rows(b'STYLEOS S864 BATCH-END')
        if any(len(x)!=rounds for x in (req,beg,end,rb,re)) or len(slices)!=27*rounds:raise ValueError('four live launch/poll count')
        tags=(b'STYLEOS S864 ',b'STYLEOS S848 ',b'STYLEOS S849 ',b'STYLEOS S860 ',b'STYLEOS S861T ',b'STYLEOS S862',b'STYLEOS S863M ')
        workload=[]
        dispatch_before=0;session=None;prev=lines.index(b'STYLEOS DESKTOP READY')
        for i in range(rounds):
            if tuple(row(req[i],b'STYLEOS S863B REQUEST',('root','completed','dispatches','accepted')).values())!=(root,i,N*i,1):raise ValueError('four request')
            begin=row(beg[i],b'STYLEOS S863L BEGIN',('root','dispatches','frame','sessions'));se=begin['sessions']
            if tuple(begin.values())!=(root,N*i,fnv,se) or tuple(row(end[i],b'STYLEOS S863L DONE',('root','dispatches','frame','sessions')).values())!=(root,N*(i+1),fnv,se) or (session is not None and session!=se):raise ValueError('four live session')
            session=se
            ids=[lines.index(x) for x in (req[i],beg[i],rb[i],slices[i*27],re[i],slices[i*27+26],end[i])]
            if prev>=ids[0] or ids!=sorted(ids):raise ValueError('four live launch order')
            prev=ids[-1];workload.extend(x for x in lines[ids[1]+1:ids[-1]] if x.startswith(tags))
            windows=4
            for j,(_,kind) in enumerate(list(order())+[(0,'cleanup')]):
                cleanup=kind=='cleanup'
                if kind=='end':windows-=1
                n=i*27+j+1;want=(n,dispatch_before,dispatch_before+int(not cleanup),0 if cleanup else 12,0 if cleanup else 4,0 if cleanup else windows,int(cleanup))
                if tuple(row(slices[n-1],b'STYLEOS S863P SLICE',('n','before','after','used','processes','windows','done')).values())!=want:raise ValueError('four live poll')
                dispatch_before=want[2]
        if workload!=[x for x in lines if x.startswith(tags)] or dispatch_before!=N*rounds or rows(b'STYLEOS S863 EXIT'):raise ValueError('four live workload scope')
    else:
        root,events=verify_cold(lines,prefix,data,site,ref,out,True,repeatable=True,live_preview=preview,mixed=preview,round_count=rounds)
    pointer=verify_pointer_clock(lines,root) if preview else None
    smp_refusal=refuse_smp_preview(cmd,vars,out,iso,iso_before) if preview else None
    (out/'results.json').write_text(json.dumps(dict(status='PASS',four_live=args.four,diagnostic_only=True,autonomous_boot=True,storage_authorized=False,launch_rounds=rounds,host_request_to_done_ns=elapsed,root=root,cpu_events=events,uart_sha256=sha(raw),trace_sha256=sha(data),efi_sha256=sha(efi.read_bytes()),boot_sha256=before,preview_iso=preview,iso_sha256=iso_before if preview else None,iso_media=media_evidence,smp_refusal=smp_refusal,pointer_clock=pointer,host_exit=rc),indent=2))
    print('S863 autonomous cold shell PASS: no pre-ready input or disk permission, {0} CPU events, {1} independent launch cycles, no storage writes'.format(len(events),rounds))
if __name__=='__main__':main()
