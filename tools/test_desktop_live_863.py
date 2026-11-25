#!/usr/bin/env python3
"""Actual CPL3 runs requested by a live shell; diagnostic F9 and F12 only."""
import argparse,gzip,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from runtime_live_863_verify import site_check,verify,trace_check,frames_check,refusals
from test_desktop_interactive_863 import wait
from test_desktop_frame_852_host import reference
from test_desktop_v0 import Desktop
from test import ROOT,firmware_path

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-live-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
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
            for i,(key,name) in enumerate((('f6','language'),(None,'cursor'),('f5','notes'),('a','text'),('f9','after-runtime'),('f10','restart')),1):
                if key:d.key(key)
                else:d.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}])
                wait(proc,serial,b'STYLEOS S863 EVENT ',i)
                if key=='f9':wait(proc,serial,b'STYLEOS S863L DONE ')
                d.screen(name)
            # Actual test-only guest exit, not a fake host success.
            d.call('human-monitor-command',**{'command-line':'sendkey f12 40'});rc=proc.wait(timeout=30)
        finally:
            d.ready.close()
            if proc.poll() is None:proc.kill();proc.wait()
            proc.stdin.close();proc.stdout.close()
    frames,frame_fnv=frames_check(out);raw=serial.read_bytes();ok,obs=verify(raw,rc,site,ref,frame_fnv)
    if not ok:raise RuntimeError(obs)
    data=trace.read_bytes();events=trace_check(data.decode('ascii'),obs);assert len(events)==25
    with gzip.open(out/'cpu-trace.log.gz','wb') as f:f.write(data)
    trace.unlink()
    denied=refusals(raw,rc,site,ref,frame_fnv)
    result=dict(status='PASS',stage=863,live_shell=True,frame_fnv=frame_fnv,diagnostic_only=True,ordinary_desktop_verified=False,production_cr3_authorized=False,storage_authorized=False,clock_authorized=False,exit=rc,site=site,reference=ref,cpu_events=events,observations=obs,frames=frames,mutation_refusals=denied,mutation_cases=denied,uart_sha256=hashlib.sha256(raw).hexdigest(),trace_sha256=hashlib.sha256(data).hexdigest(),efi_sha256=hashlib.sha256(efi.read_bytes()).hexdigest(),map_sha256=hashlib.sha256(mp.read_bytes()).hexdigest())
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print('S863L actual live shell PASS: 25 CPU events, 7 input batches, 7 QMP frames; refusal cases',denied)
if __name__=='__main__':main()
