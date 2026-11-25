#!/usr/bin/env python3
"""S863 fixed disposable PIO write and separate-boot readback."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from desktop_switch_851_site import bp_site
from test_desktop_frame_852_host import ROOT,reference
from test_desktop_input_853 import cursor_reference,wait
from test_desktop_rtc_854 import verify as rtc_verify
from storage_topology_856_verify import verify as topology_verify
from block_fixture_856_verify import CONTROLLER, DEVICE, RAW, FILE
from overlay_fixture_856 import TOP,TOPFILE,verify_map
from probe_storage_fixture_856 import QMP,dump
from sector_evidence_856 import fixture
from guest_fixture_857 import scope_fixture,verify_legacy_io
from bmdma_857_verify import verify_bar
from ata_read_857_verify import verify_read
from ata_stats_857 import verify_counts
from legacy_ports_857 import pair_dir,verify_pair,verify_ready
from probe_storage_fixture_856 import probe


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--reboot-from',type=Path);args=p.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);build=out/'build';serial=out/'serial.log'
    data=fixture(19);base=out/'fixture.raw';top=out/'disposable.qcow2';base.write_bytes(data);base.chmod(0o444)
    write=args.reboot_from is None
    payload=bytes((i^0xa5)&255 for i in range(512));expected=data[:2049*512]+payload+data[2050*512:]
    report={'status':'FAIL','ata_access':'diagnostic-write' if write else 'diagnostic-reboot-read','admission':False,'top_writable':True}
    img=shutil.which('qemu-img');q=shutil.which('qemu-system-x86_64')
    from test import firmware_path
    code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    def image(*argv):return subprocess.check_output([img,*argv],stderr=subprocess.STDOUT,timeout=25)
    def check_image(tag):
        rows=json.loads(image('map','--output=json','-f','qcow2',str(top)));dump(out/(tag+'-map.json'),rows);verify_map(rows)
        (out/(tag+'-compare.log')).write_bytes(image('compare','-f','raw','-F','qcow2',str(base),str(top)))
    def snapshot(m,tag,final=False):
        o={}
        for key,method,kw in [('status','query-status',{}),('pci','query-pci',{}),('blocks','query-block',{}),('nodes','query-named-block-nodes',{}),('graph','x-debug-query-block-graph',{}),('qtree','human-monitor-command',{'command-line':'info qtree'}),('mtree','human-monitor-command',{'command-line':'info mtree -f -o'}),('unit','qom-get',{'path':'/machine/peripheral/'+DEVICE,'property':'unit'}),('stats','query-blockstats',{'query-nodes':False})]:
            o[key]=m.call(method,**kw);dump(out/(tag+'-'+key+'.json'),o[key])
        if final:
            # Preserve raw evidence. The original verifier demands zero writes;
            # validate actual counters separately before checking topology.
            import copy
            stat=next(r['stats'] for r in o['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
            assert stat['rd_operations']==(6 if write else 5) and stat['rd_bytes']==512*(6 if write else 5)
            assert stat['wr_operations']==int(write) and stat['wr_bytes']==512*int(write)
            assert (stat['flush_operations']>=1 if write else stat['flush_operations']==0)
            assert all(stat[k]==0 for k in ('unmap_operations','unmap_bytes','failed_rd_operations','invalid_rd_operations','failed_wr_operations','invalid_wr_operations','failed_flush_operations','invalid_flush_operations'))
            projected=copy.deepcopy(o)
            ps=next(r['stats'] for r in projected['stats'] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
            for k in ('wr_operations','wr_bytes','flush_operations'):ps[k]=0
            proof=scope_fixture(projected,base,top)
            proof['frontend_mutations']=int(write)
        else:proof=scope_fixture(o,base,top)
        proof.update(verify_legacy_io(o['mtree']));return o,proof
    try:
        if not(q and img and code and vars):raise RuntimeError('QEMU/OVMF/qemu-img required')
        version=subprocess.check_output([q,'--version'],text=True);(out/'version.txt').write_text(version)
        if not version.startswith('QEMU emulator version 8.2.2 '):raise RuntimeError('unvalidated QEMU fixture version')
        for mode in ('default','piix3'):
            if probe(q,out/('control-'+mode),mode)['status']!='PASS':raise RuntimeError('control probe failed')
        report['port_delta']=verify_pair(pair_dir(out/'control-default'),pair_dir(out/'control-piix3'))
        ref=reference(out/'first-gcc','gcc');assert ref==reference(out/'first-san','clang',True)
        cur=cursor_reference(out/'cursor-gcc','gcc');assert cur==cursor_reference(out/'cursor-san','clang',True)
        with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-storage-pio-runtime-863','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
        efi=build/'x86_64/BOOTX64.EFI';mp=build/'x86_64/kernel.map';site=bp_site(efi.read_bytes(),mp.read_text());shutil.copyfile(vars,out/'OVMF_VARS.fd')
        if write:
            (out/'create.log').write_bytes(image('create','-f','qcow2','-F','raw','-b',str(base),str(top)));check_image('before')
        else:
            previous=args.reboot_from.resolve()
            prior=json.loads((previous/'result.json').read_text())
            assert prior['status']=='PASS' and prior['write'] is True
            assert (previous/'fixture.raw').read_bytes()==data
            shutil.copyfile(previous/'disposable.qcow2',top)
            assert hashlib.sha256(top.read_bytes()).hexdigest()==prior['overlay_sha256']
            (out/'rebase.log').write_bytes(image('rebase','-u','-f','qcow2','-F','raw','-b',str(base),str(top)))
            image('convert','-f','qcow2','-O','raw',str(top),str(out/'before.raw'))
            assert (out/'before.raw').read_bytes()==expected
        cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-rtc','base=2026-11-24T19:34:00,clock=vm','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos,bootindex=1','-device','piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5']
        for n in ({'driver':'file','filename':str(base),'node-name':FILE,'read-only':True},{'driver':'raw','file':FILE,'node-name':RAW,'read-only':True},{'driver':'file','filename':str(top),'node-name':TOPFILE,'read-only':False},{'driver':'qcow2','file':TOPFILE,'backing':RAW,'node-name':TOP,'read-only':False}):cmd+=['-blockdev',json.dumps(n)]
        cmd+=['-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive={TOP}'];dump(out/'command.json',cmd)
        with (out/'qemu.log').open('wb') as err,(out/'qmp.jsonl').open('w') as transcript:
            proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,bufsize=0)
            try:
                m=QMP(proc,transcript);assert 'QMP' in m.receive();m.call('qmp_capabilities')
                prelaunch,report['prelaunch']=snapshot(m,'prelaunch');m.call('cont')
                wait(proc,serial,b'STYLEOS S853 READY ');m.call('stop')
                ready,report['owned_root_ready']=snapshot(m,'ready');verify_ready(ready);verify_bar(ready['pci'],ready['mtree']);m.call('cont')
                m.call('human-monitor-command',**{'command-line':'sendkey a 40'});wait(proc,serial,b'STYLEOS S853 KEY ')
                m.call('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':20}}]);wait(proc,serial,b'STYLEOS S857 ATA-READY ');m.call('stop')
                after,report['post_read']=snapshot(m,'post-read');verify_ready(after);verify_bar(after['pci'],after['mtree']);report['frontend_counts']=verify_counts(prelaunch['stats'],ready['stats'],after['stats']);m.call('cont')
                m.call('human-monitor-command',**{'command-line':'sendkey '+('b' if write else 'r')+' 40'})
                wait(proc,serial,b'STYLEOS S863D READY');m.call('stop')
                committed,report['committed']=snapshot(m,'committed',final=True);verify_ready(committed);verify_bar(committed['pci'],committed['mtree']);m.call('cont')
                m.call('human-monitor-command',**{'command-line':'sendkey c 40'});rc=proc.wait(timeout=30)
            finally:
                if proc.poll() is None:proc.kill();proc.wait(timeout=8)
        raw=serial.read_bytes();ok,obs=rtc_verify(raw,rc,site['rva'],ref,cur,19*3600+34*60)
        if not ok:raise RuntimeError(obs)
        t=topology_verify(raw,obs['active']['root'],ready['pci']);assert t['pci'].get(40,(0,0))[0]==0x70108086 and 40 in t['ide_config']
        report['ata']=verify_read(raw,obs['active']['root'],t,data)
        report.update(exit=rc,root=obs['active']['root'],storage_topology=t,site=site,uart_sha256=hashlib.sha256(raw).hexdigest(),efi_sha256=hashlib.sha256(efi.read_bytes()).hexdigest(),map_sha256=hashlib.sha256(mp.read_bytes()).hexdigest())
        rows=[x for x in raw.splitlines() if x.startswith(b'STYLEOS S863D ')]
        prefix=f'STYLEOS S863D DATA root=0x{obs["active"]["root"]:016x} write=0x{int(write):016x} lba=0x{2049:016x} bytes='.encode()
        assert rows==[prefix+payload.hex().encode(),b'STYLEOS S863D READY',b'STYLEOS S863D POLICY disposable=yes dma=no ordinary-cr3=deny']
        image('convert','-f','qcow2','-O','raw',str(top),str(out/'after.raw'))
        assert (out/'after.raw').read_bytes()==expected
        (out/'check.log').write_bytes(image('check','-f','qcow2',str(top)))
        report['overlay_sha256']=hashlib.sha256(top.read_bytes()).hexdigest()
        report['exact_media_sha256']=hashlib.sha256(expected).hexdigest()
        report['write']=write
        report['status']='PASS'
    except Exception as exc:
        report['error']=str(exc)
        if isinstance(exc,subprocess.CalledProcessError):report['command_output']=(exc.output or b'').decode(errors='replace')[-4096:]
    finally:
        report['before_sha256']=hashlib.sha256(data).hexdigest();report['after_sha256']=hashlib.sha256(base.read_bytes()).hexdigest()
        if base.read_bytes()!=data:report.update(status='FAIL',error='immutable backing changed')
        dump(out/'result.json',report);print(report)
    return int(report['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
