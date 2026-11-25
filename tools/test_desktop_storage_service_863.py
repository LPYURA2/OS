#!/usr/bin/env python3
"""Real shell note store, disposable guarded PIO overlay, two distinct boots."""
import argparse,hashlib,json,shutil,struct,subprocess,sys
from pathlib import Path
from make_desktop_data import provision
from test_desktop_v0 import Desktop
from test_desktop_interactive_863 import wait
from test import ROOT,firmware_path
from block_fixture_856_verify import CONTROLLER,DEVICE,RAW,FILE
from overlay_fixture_856 import TOP,TOPFILE
from guest_fixture_857 import scope_fixture,verify_legacy_io
from legacy_ports_857 import verify_ready
from bmdma_857_verify import verify_bar
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build';base=out/'legacy.raw';top=out/'data.qcow2'
    provision(base);original=base.read_bytes();base.chmod(0o444)
    q=shutil.which('qemu-system-x86_64');img=shutil.which('qemu-img');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and img and code and vars):raise RuntimeError('real QEMU mandatory')
    if not subprocess.check_output([q,'--version'],text=True).startswith('QEMU emulator version 8.2.2 '):raise RuntimeError('unvalidated fixture')
    def image(*a):return subprocess.check_output([img,*a],stderr=subprocess.STDOUT,timeout=30)
    image('create','-f','qcow2','-F','raw','-b',str(base),str(top))
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-storage-service-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    boot=build/'x86_64/styleos.img';boot_sha=sha(boot.read_bytes())
    def run(name,write):
        folder=out/name;folder.mkdir();serial=folder/'serial.log';shutil.copyfile(vars,folder/'OVMF_VARS.fd')
        cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={folder/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={boot}','-device','virtio-blk-pci,drive=styleos,bootindex=1','-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
        for node in ({'driver':'file','filename':str(base),'node-name':FILE,'read-only':True},{'driver':'raw','file':FILE,'node-name':RAW,'read-only':True},{'driver':'file','filename':str(top),'node-name':TOPFILE,'read-only':False},{'driver':'qcow2','file':TOPFILE,'backing':RAW,'node-name':TOP,'read-only':False}):cmd+=['-blockdev',json.dumps(node)]
        cmd+=['-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={TOP}']
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
                d.screen('note');d.call('stop');final=snapshot('final')
                verify_ready(final);verify_bar(final['pci'],final['mtree'])
                stats=next(r['stats'] for r in final['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
                if stats['wr_operations']!=(2 if write else 0) or stats['wr_bytes']!=(1024 if write else 0) or (write and stats['flush_operations']<2) or (not write and stats['flush_operations']):raise ValueError('frontend mutation count')
                d.call('cont');d.call('human-monitor-command',**{'command-line':'sendkey f12 40'})
                if proc.wait(timeout=30)!=33:raise ValueError('real guest exit')
            finally:
                d.ready.close()
                if proc.poll() is None:proc.kill();proc.wait()
                proc.stdin.close();proc.stdout.close()
        raw=serial.read_bytes()
        if b' FAIL ' in raw or b'STYLEOS DESKTOP DOCUMENT SAVE ERROR' in raw:raise ValueError('guest failure')
        lines=raw.splitlines();commits=[x for x in lines if x.startswith(b'STYLEOS S863S COMMIT ')]
        if len(commits)!=(2 if write else 0):raise ValueError('commit count')
        if write and [int(x.split()[-1].split(b'=')[1],16) for x in commits]!=[20,19]:raise ValueError('payload then header')
        if not write:
            opened=lines.index(b'STYLEOS DESKTOP DOCUMENT OPEN')
            ev=next(x for x in lines[opened+1:] if x.startswith(b'STYLEOS S863 EVENT '))
            h=((14695981039346656037^97)*1099511628211)&((1<<64)-1)
            if b' text=0x0000000000000001 ' not in ev or f' text-hash=0x{h:016x} '.encode() not in ev:raise ValueError('reopened note content')
        return {'uart_sha256':sha(raw),'write':write,'writes':stats['wr_operations']}
    first=run('first',True)
    expected=bytearray(original);payload=b'a'
    header=struct.pack('<8sIIBB2x',b'STYLED01',1,1,0,1);h=2166136261
    for c in header+payload:h=((h^c)*16777619)&0xffffffff
    expected[19*512:20*512]=(header+struct.pack('<I',h)).ljust(512,b'\0');expected[20*512:21*512]=payload.ljust(512,b'\0')
    def check(tag):
        path=out/(tag+'.raw');image('convert','-f','qcow2','-O','raw',str(top),str(path))
        if path.read_bytes()!=expected or base.read_bytes()!=original or sha(boot.read_bytes())!=boot_sha:raise ValueError('exact disk contents')
    check('after-write');second=run('second',False);check('after-reboot')
    (out/'results.json').write_text(json.dumps(dict(status='PASS',diagnostic_only=True,ordinary_desktop_verified=False,first=first,second=second,base_sha256=sha(original),media_sha256=sha(expected),boot_sha256=boot_sha),indent=2))
    print('S863 shell note PIO PASS: real save, separate boot reopen, exact media, zero reboot writes')
if __name__=='__main__':main()
