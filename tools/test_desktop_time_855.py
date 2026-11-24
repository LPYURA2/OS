#!/usr/bin/env python3
"""S855 terminal Q35/TCG PIT/TSC and RTC crosscheck fixture."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from desktop_switch_851_site import bp_site
from test_desktop_frame_852_host import ROOT,reference
from test_desktop_v0 import Desktop
from test import firmware_path
from test_desktop_input_853 import verify as input_verify,cursor_reference,wait
from test_desktop_rtc_854 import verify as rtc_verify
from desktop_time_855_verify import verify_record,POLICY

def verify(raw,rc,rva,ref,post,base):
    ok,obs=rtc_verify(raw,rc,rva,ref,post,base)
    if not ok:return False,obs
    try:
        obs['monotonic']=verify_record(raw,obs['active']['root'],base)
        if not obs['monotonic']['min_hz']<=1_000_000_000<=obs['monotonic']['max_hz']:
            raise ValueError('855 icount reference 1GHz outside measured interval')
        return True,obs
    except ValueError as e:return False,str(e)

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--hour',required=True,type=int,choices=(12,19));args=p.parse_args();base=args.hour*3600+34*60
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    ref=reference(out/'first-gcc','gcc');assert ref==reference(out/'first-san','clang',True)
    post=cursor_reference(out/'cursor-gcc','gcc');assert post==cursor_reference(out/'cursor-san','clang',True)
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-time-855','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=bp_site(efi.read_bytes(),mp.read_text())
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True)
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-icount','shift=3,align=off,sleep=off','-smp','1','-m','128M','-rtc',f'base=2026-11-24T{args.hour:02d}:34:00,clock=vm','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos']
    (out/'command.json').write_text(json.dumps(cmd)+'\n')
    with (out/'qemu.log').open('wb') as log:
        proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log)
        try:
            d=Desktop(proc,serial,out);assert 'QMP' in d.receive();d.call('qmp_capabilities')
            wait(proc,serial,b'STYLEOS S853 READY ')
            d.call('human-monitor-command',**{'command-line':'sendkey a 40'})
            wait(proc,serial,b'STYLEOS S853 KEY ')
            d.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}])
            rc=proc.wait(timeout=120)
        finally:
            if proc.poll() is None:proc.kill();proc.wait()
    raw=serial.read_bytes();ok,obs=verify(raw,rc,site['rva'],ref,post,base)
    result={'status':'PASS' if ok else 'FAIL','exit':rc,'site':site,'reference':ref,'cursor_reference':post,'uart_sha256':hashlib.sha256(raw).hexdigest(),'efi_sha256':hashlib.sha256(efi.read_bytes()).hexdigest(),'map_sha256':hashlib.sha256(mp.read_bytes()).hexdigest(),'rtc_fixture_base':base,'clock_fixture':'icount-shift-3','observations':obs}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if not ok:raise RuntimeError(obs)
    record=next(x for x in raw.splitlines() if x.startswith(b'STYLEOS S855 MONO '))
    edge=next(x for x in raw.splitlines() if x.startswith(b'STYLEOS S855 EDGE '))
    cal=next(x for x in raw.splitlines() if x.startswith(b'STYLEOS S855 CAL '))
    cases=[(raw,0),(raw.replace(POLICY,b''),33),(raw+POLICY+b'\n',33),(raw.replace(POLICY,POLICY.replace(b'pit2=stopped',b'pit2=running')),33),(raw.replace(record,b''),33),(raw.replace(record,record.split(b' ns=')[0]+b' ns=0x0000000000000000'),33),(raw.replace(edge,b''),33),(raw.replace(cal,b''),33)]
    for i,(bad,exitcode) in enumerate(cases):
        if verify(bad,exitcode,site['rva'],ref,post,base)[0]:raise RuntimeError('accepted mutation '+str(i))
    print('S855 PIT/TSC RTC crosscheck QEMU PASS; actual-capture refusals',len(cases))
if __name__=='__main__':main()
