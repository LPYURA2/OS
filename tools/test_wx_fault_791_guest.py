#!/usr/bin/env python3
"""Two distinct x86 hardware negative probes, QEMU/OVMF required, no fake PASS."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from test import firmware_path
from wx_fault_791_verify import verify
ROOT=Path(__file__).resolve().parents[1]

def run_one(out:Path,mode:int):
    out.mkdir(parents=True,exist_ok=True)
    built=out/'build'
    with (out/'compile.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
            '--test-wx-write-791' if mode==1 else '--test-wx-exec-791',
            '--output',str(built)],check=True,stdout=log,stderr=subprocess.STDOUT)
    qemu=shutil.which('qemu-system-x86_64')
    code,variables=(firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not code or not variables:
        raise RuntimeError('real QEMU + OVMF mandatory; no synthetic PASS')
    variables_copy=out/'OVMF_VARS.fd'
    shutil.copyfile(variables,variables_copy)
    serial=out/'serial.log'
    command=[qemu,'-accel','tcg,thread=single','-m','128M','-display','none',
         '-serial',f'file:{serial}','-monitor','none','-net','none',
         '-no-reboot','-machine','q35','-vga','none',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={variables_copy}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={built/"x86_64"/"styleos.img"}',
         '-device','virtio-blk-pci,drive=styleos']
    try:
        process=subprocess.run(command,capture_output=True,text=True,timeout=90)
        rc=process.returncode
        (out/'qemu.log').write_text(process.stdout+process.stderr)
    except subprocess.TimeoutExpired as error:
        rc=None
        (out/'qemu.log').write_text('TIMEOUT '+str(error))
    raw=serial.read_bytes() if serial.exists() else b''
    ok,reason=verify(raw,rc,mode)
    result={'status':'PASS' if ok else 'FAIL','reason':reason,
       'mode':'rx-write' if mode==1 else 'nx-exec','qemu_exit_code':rc,
       'real_hardware_probe':ok,'pf_on_ist2_verified':ok,
       'supervisor_write_to_rx_refused':ok if mode==1 else None,
       'nx_instruction_fetch_refused':ok if mode==2 else None,
       'production_wx_verified':False,'desktop_runtime_affected':False,
       'uart_sha256':hashlib.sha256(raw).hexdigest(),
       'efi_sha256':hashlib.sha256((built/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS 791',result['mode'],result['status'],reason,flush=True)
    if not ok:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--mode',choices=['write','exec'],required=True)
    a=p.parse_args()
    run_one(a.output,1 if a.mode=='write' else 2)
if __name__=='__main__':main()
