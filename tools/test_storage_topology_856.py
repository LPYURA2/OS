#!/usr/bin/env python3
"""S856 terminal PCI topology evidence only, default Q35 or explicit PIIX3."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from desktop_switch_851_site import bp_site
from test_desktop_frame_852_host import ROOT,reference
from test_desktop_v0 import Desktop
from test import firmware_path
from test_desktop_input_853 import verify as input_verify,cursor_reference,wait
from storage_topology_856_verify import verify as topology_verify
from test_desktop_rtc_854 import verify as rtc_verify

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--hour',required=True,type=int,choices=(12,19));p.add_argument('--piix3',action='store_true');args=p.parse_args();base=args.hour*3600+34*60
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    ref=reference(out/'first-gcc','gcc');assert ref==reference(out/'first-san','clang',True)
    post=cursor_reference(out/'cursor-gcc','gcc');assert post==cursor_reference(out/'cursor-san','clang',True)
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-storage-topology-856','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=bp_site(efi.read_bytes(),mp.read_text())
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True)
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-rtc',f'base=2026-11-24T{args.hour:02d}:34:00,clock=vm','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos']
    if args.piix3:cmd += ['-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
    (out/'command.json').write_text(json.dumps(cmd)+'\n')
    with (out/'qemu.log').open('wb') as log:
        proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log)
        try:
            d=Desktop(proc,serial,out);assert 'QMP' in d.receive();d.call('qmp_capabilities')
            wait(proc,serial,b'STYLEOS S853 READY ')
            pci=d.call('query-pci');(out/'pci.json').write_text(json.dumps(pci,indent=2)+'\n')
            d.call('human-monitor-command',**{'command-line':'sendkey a 40'})
            wait(proc,serial,b'STYLEOS S853 KEY ')
            d.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}])
            rc=proc.wait(timeout=30)
        finally:
            if proc.poll() is None:proc.kill();proc.wait()
    raw=serial.read_bytes();ok,obs=rtc_verify(raw,rc,site['rva'],ref,post,base)
    if ok:
        try:obs['storage_topology']=topology_verify(raw,obs['active']['root'],pci)
        except ValueError as exc:ok=False;obs=str(exc)
    result={'status':'PASS' if ok else 'FAIL','exit':rc,'site':site,'reference':ref,'cursor_reference':post,'uart_sha256':hashlib.sha256(raw).hexdigest(),'efi_sha256':hashlib.sha256(efi.read_bytes()).hexdigest(),'map_sha256':hashlib.sha256(mp.read_bytes()).hexdigest(),'rtc_fixture_base':base,'observations':obs}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if not ok:raise RuntimeError(obs)
    if args.piix3 and obs['storage_topology']['pci'].get(40,(0,0))[0]!=0x70108086:raise RuntimeError('explicit PIIX3 missing at BDF 00:05.0')
    print('S856 topology census PASS; ATA access NOT RUN; admission denied',obs['storage_topology'])
if __name__=='__main__':main()
