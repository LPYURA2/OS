#!/usr/bin/env python3
"""Dedicated, terminal QEMU/OVMF 784 LGDT-LTR-STR trial. Never model-pass."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from test import firmware_path
from tss_ltr_784_verify import verify
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    built=a.output/'build'
    subprocess.run([sys.executable,str(ROOT/'tools/build.py'),
        '--arch','x86_64','--test-tss-ltr-784','--output',str(built)],
        check=True,stdout=(a.output/'compile.log').open('w'))
    qemu=shutil.which('qemu-system-x86_64')
    code,variables=(firmware_path('OVMF_CODE','OVMF_CODE'),
                    firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not code or not variables:
        raise RuntimeError('784 needs real QEMU and OVMF; no synthetic PASS')
    vars_copy=a.output/'OVMF_VARS.fd'
    shutil.copyfile(variables,vars_copy)
    serial=a.output/'serial.log'
    cmd=[qemu,'-accel','tcg,thread=single','-m','128M','-display','none',
         '-serial',f'file:{serial}','-monitor','none','-net','none',
         '-no-reboot','-machine','q35','-vga','none',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={vars_copy}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={built/"x86_64"/"styleos.img"}',
         '-device','virtio-blk-pci,drive=styleos']
    try:
        r=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
        rc=r.returncode
        (a.output/'qemu.log').write_text(r.stdout+r.stderr)
    except subprocess.TimeoutExpired as exc:
        rc=None
        (a.output/'qemu.log').write_text('TIMEOUT: '+str(exc))
    uart=serial.read_bytes() if serial.exists() else b''
    passed,reason=verify(uart,rc)
    result={'status':'PASS' if passed else 'FAIL','details':reason,
        'qemu_exit_code':rc,'expected_exit_code':33,
        'hardware_test':True,'firmware_return':False if passed else None,
        'guard_pages_verified':False,'ist_trap_delivery_verified':False,
        'desktop_runtime_affected':False,
        'uart_sha256':hashlib.sha256(uart).hexdigest(),
        'efi_sha256':hashlib.sha256((built/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS 784 TERMINAL QEMU '+result['status']+' — '+reason,flush=True)
    if not passed:raise SystemExit(1)
if __name__=='__main__':main()
