#!/usr/bin/env python3
"""S853 Q35/TCG-only PS/2 input fixture. Real QMP events and framebuffer readback."""
import argparse,hashlib,json,re,shutil,subprocess,sys,time
from pathlib import Path
from desktop_frame_852_verify import verify as prior, POLICY as FIRST
from desktop_switch_851_site import bp_site
from test_desktop_frame_852_host import ROOT,SOURCES,reference
from test_desktop_v0 import Desktop
from test import firmware_path

POLICY=b'STYLEOS S853 POLICY polling=yes irq=no storage=no clock=no reset=no app-admission=no production-cr3=deny'
def cursor_reference(out,cc,san=False):
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DSTYLEOS_TEST_DESKTOP_FRAME_852=1','-DSTYLEOS_TEST_DESKTOP_INPUT_853=1','-I',str(ROOT/'include'),*(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if san else []),*[str(ROOT/'kernel'/f'{s}.c') for s in SOURCES],str(ROOT/'tests/test_desktop_input_853.c'),'-o',str(out)],check=True)
    values=tuple(map(int,subprocess.check_output([str(out)],text=True).split()))
    if len(values)!=2 or not 0<values[0]<=340:raise RuntimeError('cursor reference')
    return values

def verify(raw,rc,rva,ref,post):
    try:
        ok,obs=prior(raw,rc,rva,ref)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();records=[x for x in lines if x.startswith(b'STYLEOS S853 ')]
        if len(records)!=4 or records[-1]!=POLICY or lines.index(FIRST)>=lines.index(records[0]):raise ValueError('853 policy/order')
        root=obs['active']['root']
        schema=(('READY',('root','lpc','input'),(root,0x29188086,3)),('KEY',('root','key','modifiers'),(root,0x1e,0)),('MOUSE',('root','dx','dy','buttons','writes','fnv'),(root,20,0,0,*post)))
        for line,(tag,keys,want) in zip(records,schema):
            m=re.fullmatch(('STYLEOS S853 '+tag+' ').encode()+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),line)
            if not m or tuple(int(x,16) for x in m.groups())!=want:raise ValueError('853 '+tag)
        if not 0<post[0]<=340 or post[1]==ref['fnv']:raise ValueError('853 damage')
        return True,obs
    except (TypeError,ValueError,KeyError,IndexError) as e:return False,str(e)

def wait(proc,serial,tag):
    deadline=time.monotonic()+35
    while time.monotonic()<deadline:
        if serial.exists() and any(x.startswith(tag) for x in serial.read_bytes().splitlines()):return
        if proc.poll() is not None:break
        time.sleep(.02)
    raise RuntimeError('missing '+repr(tag))

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    ref=reference(out/'first-gcc','gcc');assert ref==reference(out/'first-san','clang',True)
    post=cursor_reference(out/'cursor-gcc','gcc');assert post==cursor_reference(out/'cursor-san','clang',True)
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-input-853','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=bp_site(efi.read_bytes(),mp.read_text())
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True)
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos']
    with (out/'qemu.log').open('wb') as log:
        proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log)
        try:
            d=Desktop(proc,serial,out);d.receive();d.call('qmp_capabilities')
            wait(proc,serial,b'STYLEOS S853 READY ')
            d.call('human-monitor-command',**{'command-line':'sendkey a 40'})
            wait(proc,serial,b'STYLEOS S853 KEY ')
            d.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}])
            rc=proc.wait(timeout=30)
        finally:
            if proc.poll() is None:proc.kill();proc.wait()
    raw=serial.read_bytes();ok,obs=verify(raw,rc,site['rva'],ref,post)
    result={'status':'PASS' if ok else 'FAIL','exit':rc,'site':site,'reference':ref,'cursor_reference':post,'uart_sha256':hashlib.sha256(raw).hexdigest(),'efi_sha256':hashlib.sha256(efi.read_bytes()).hexdigest(),'map_sha256':hashlib.sha256(mp.read_bytes()).hexdigest(),'observations':obs}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if not ok:raise RuntimeError(obs)
    cases=[(raw,0),(raw.replace(POLICY,b''),33),(raw+POLICY+b'\n',33),(raw.replace(b'irq=no',b'irq=yes'),33),(raw.replace(b'storage=no',b'storage=yes'),33),(raw.replace(b'key=0x000000000000001e',b'key=0x000000000000001f'),33),(raw.replace(b'dx=0x0000000000000014',b'dx=0x0000000000000015'),33)]
    for i,(bad,exitcode) in enumerate(cases):
        if verify(bad,exitcode,site['rva'],ref,post)[0]:raise RuntimeError('accepted mutation '+str(i))
    print('S853 QEMU PS/2 input and cursor damage PASS; actual-capture refusals',len(cases))
if __name__=='__main__':main()
