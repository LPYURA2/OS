#!/usr/bin/env python3
"""Real S861 Desktop/child-root roundtrips; no synthetic fallback or production activation."""
import argparse,gzip,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from runtime_user_861_verify import verify,trace_check,user_site
from test_desktop_frame_852_host import reference
from desktop_switch_851_site import bp_site
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-runtime-user-861','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    efi=build/'x86_64/BOOTX64.EFI';map_file=build/'x86_64/kernel.map';site=user_site(efi.read_bytes(),map_file)
    (out/'site.json').write_text(json.dumps(site,indent=2)+'\n')
    ref=reference(out/'host-reference',cc='gcc');san=reference(out/'host-reference-san',cc='clang',sanitize=True)
    if ref!=san:raise RuntimeError('host renderer disagreement')
    (out/'reference.json').write_text(json.dumps(ref,indent=2)+'\n')
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True)
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos']
    trace=out/'cpu-trace.log';cmd += ['-d','int,guest_errors','-D',str(trace)]
    (out/'qemu-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
    rc=None
    try:
        run=subprocess.run(cmd,capture_output=True,timeout=100);rc=run.returncode;(out/'qemu.log').write_bytes(run.stdout+run.stderr)
    except subprocess.TimeoutExpired as e:(out/'qemu.log').write_text('TIMEOUT '+str(e))
    raw=serial.read_bytes() if serial.exists() else b'';ok,obs=verify(raw,rc,site,ref)
    events=None;trace_sha=None
    if trace.exists():
        data=trace.read_bytes();trace_sha=hashlib.sha256(data).hexdigest()
        with gzip.open(out/'cpu-trace.log.gz','wb') as dest:dest.write(data)
        trace.unlink()
        if ok:
            try:events=trace_check(data.decode('ascii'),obs)
            except (ValueError,KeyError,TypeError,IndexError) as exc:ok=False;obs=str(exc)
    elif ok:ok=False;obs='missing CPU trace'
    (out/'results.json').write_text(json.dumps({'stage':861,'cpu_events':events,'trace_sha256':trace_sha,'cpl3_verified':ok,'timer_verified':False,'status':'PASS' if ok else 'FAIL','qemu_exit_code':rc,'detached_preflight_verified':ok,'desktop_root_switch':ok,'framebuffer_write_restored':ok,'hardware_ist_verified':ok,'desktop_first_frame_verified':ok,'runtime_services_on_owned_root':ok,'bounded_arena_on_owned_root':ok,'child_roots_cpl0_verified':ok,'ordinary_desktop_verified':False,'device_io_authorized':False,'production_cr3_authorized':False,'uart_sha256':hashlib.sha256(raw).hexdigest(),'efi_sha256':hashlib.sha256((build/'x86_64/BOOTX64.EFI').read_bytes()).hexdigest(),'map_sha256':hashlib.sha256(map_file.read_bytes()).hexdigest(),'site':site,'observations':obs if ok else None,'failure':None if ok else obs},indent=2)+'\n')
    print('STYLEOS S861 real QEMU Desktop and cooperative CPL3', 'PASS' if ok else 'FAIL',obs,flush=True)
    if not ok:raise SystemExit(1)
if __name__=='__main__':main()
