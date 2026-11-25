#!/usr/bin/env python3
"""Cold boot without external permission/input, storage fail-closed, actual tasks."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from test_desktop_v0 import Desktop
from test_desktop_interactive_863 import wait,recycle_site
from test_desktop_storage_service_863 import verify_cold
from test_desktop_frame_852_host import reference
from test import ROOT,firmware_path
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-autonomous-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=recycle_site(efi.read_bytes(),mp)
    for forbidden in ('s863_disk_write','s863_disk_read','storage_commit_863','ata_runtime_write_863','disk_admit863'):
        if forbidden in mp.read_text():raise ValueError('storage backend linked '+forbidden)
    ref=reference(out/'host-reference','gcc');assert ref==reference(out/'host-reference-san','clang',True)
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';trace=out/'cpu-trace.log'
    boot=build/'x86_64/styleos.img';before=sha(boot.read_bytes())
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-rtc','base=2026-11-25T12:34:00,clock=vm','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={boot}','-device','virtio-blk-pci,drive=styleos','-d','int,guest_errors','-D',str(trace)]
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
            d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',1);d.screen('after-first-launch')
            d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',2);d.screen('after-tasks')
            for i in (3,4):
                d.key('f9');wait(proc,serial,b'STYLEOS S863L DONE ',i);d.screen(f'after-launch-{i}')
            d.call('stop');stats=d.call('query-blockstats',**{'query-nodes':False})
            (out/'stats.json').write_text(json.dumps(stats,indent=2))
            if not stats or any(x['stats']['wr_operations'] or x['stats']['wr_bytes'] for x in stats):raise ValueError('unexpected write')
            d.call('cont');d.call('human-monitor-command',**{'command-line':'sendkey f12 40'});rc=proc.wait(timeout=30)
        finally:
            d.ready.close()
            if proc.poll() is None:proc.kill();proc.wait()
            proc.stdin.close();proc.stdout.close()
    raw=serial.read_bytes();lines=raw.splitlines();data=trace.read_bytes()
    if rc!=33 or b' FAIL ' in raw or sha(boot.read_bytes())!=before:raise ValueError('actual exit or boot media')
    if lines.count(b'STYLEOS DESKTOP DATA UNAVAILABLE')!=1 or lines.count(b'STYLEOS S863 STORAGE DENIED')<1 or b'STYLEOS DESKTOP DATA MOUNT' in lines or any(x.startswith(b'STYLEOS S863S ') for x in lines):raise ValueError('storage fail-closed')
    policy=b'STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=rtc-service reset=no dma-grant=no production-cr3=deny'
    if lines.count(policy)!=1:raise ValueError('policy')
    cold=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863B COLD '))
    prefix=b'\n'.join(lines[:cold])+b'\n'
    root,events=verify_cold(lines,prefix,data,site,ref,out,True,repeatable=True)
    (out/'results.json').write_text(json.dumps(dict(status='PASS',diagnostic_only=True,autonomous_boot=True,storage_authorized=False,root=root,cpu_events=events,uart_sha256=sha(raw),trace_sha256=sha(data),efi_sha256=sha(efi.read_bytes()),boot_sha256=before),indent=2))
    print('S863 autonomous cold shell PASS: no pre-ready input or disk permission, 49 CPU events, four independent launch cycles, no storage writes')
if __name__=='__main__':main()
