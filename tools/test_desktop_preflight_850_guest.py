#!/usr/bin/env python3
"""Real QEMU preflight; no synthetic fallback or production activation."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from desktop_preflight_850_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build'
    with (out/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-desktop-preflight-850','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    q=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');vars=firmware_path('OVMF_VARS','OVMF_VARS')
    if not(q and code and vars):raise RuntimeError('real QEMU/OVMF mandatory')
    shutil.copyfile(vars,out/'OVMF_VARS.fd');serial=out/'serial.log';serial.unlink(missing_ok=True)
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}','-device','virtio-blk-pci,drive=styleos']
    rc=None
    try:
        run=subprocess.run(cmd,capture_output=True,timeout=100);rc=run.returncode;(out/'qemu.log').write_bytes(run.stdout+run.stderr)
    except subprocess.TimeoutExpired as e:(out/'qemu.log').write_text('TIMEOUT '+str(e))
    raw=serial.read_bytes() if serial.exists() else b'';ok,obs=verify(raw,rc)
    (out/'results.json').write_text(json.dumps({'stage':850,'status':'PASS' if ok else 'FAIL','qemu_exit_code':rc,'detached_preflight_verified':ok,'desktop_root_switch':False,'framebuffer_write':False,'production_cr3_authorized':False,'uart_sha256':hashlib.sha256(raw).hexdigest(),'efi_sha256':hashlib.sha256((build/'x86_64/BOOTX64.EFI').read_bytes()).hexdigest(),'observations':obs if ok else None,'failure':None if ok else obs},indent=2)+'\n')
    print('STYLEOS S850 real QEMU preflight', 'PASS' if ok else 'FAIL',obs,flush=True)
    if not ok:raise SystemExit(1)
if __name__=='__main__':main()
