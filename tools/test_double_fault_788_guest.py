#!/usr/bin/env python3
"""Isolated genuine QEMU/OVMF double-fault hardware test, fail-closed."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from test import firmware_path
from double_fault_788_verify import verify
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    built=args.output/'build'
    with (args.output/'compile.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),
            '--arch','x86_64','--test-double-fault-788','--output',str(built)],
            check=True,stdout=log,stderr=subprocess.STDOUT)
    qemu=shutil.which('qemu-system-x86_64')
    code,variables=(firmware_path('OVMF_CODE','OVMF_CODE'),
                    firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not code or not variables:
        raise RuntimeError('788 requires real QEMU and OVMF; no synthetic PASS')
    variables_copy=args.output/'OVMF_VARS.fd'
    shutil.copyfile(variables,variables_copy)
    serial=args.output/'serial.log'
    cmd=[qemu,'-accel','tcg,thread=single','-m','128M','-display','none',
         '-serial',f'file:{serial}','-monitor','none','-net','none',
         '-no-reboot','-machine','q35','-vga','none',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={variables_copy}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={built/"x86_64"/"styleos.img"}',
         '-device','virtio-blk-pci,drive=styleos']
    try:
        run=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
        rc=run.returncode
        (args.output/'qemu.log').write_text(run.stdout+run.stderr)
    except subprocess.TimeoutExpired as exc:
        rc=None
        (args.output/'qemu.log').write_text('TIMEOUT: '+str(exc))
    uart=serial.read_bytes() if serial.exists() else b''
    passed,reason=verify(uart,rc)
    result={'status':'PASS' if passed else 'FAIL','details':reason,
        'qemu_exit_code':rc,'expected_exit_code':33,
        'hardware_test':True,'genuine_double_fault_on_ist3_verified':passed,
        'guard_fault_caused_nested_exception':passed,
        'pf_gate_absent_while_df_gate_present':passed,
        'df_error_code_zero_verified':passed,
        'stack_exhaustion_recovery_verified':False,
        'desktop_runtime_affected':False,
        'uart_sha256':hashlib.sha256(uart).hexdigest(),
        'efi_sha256':hashlib.sha256((built/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS 788 QEMU '+result['status']+' — '+reason,flush=True)
    if not passed:raise SystemExit(1)
if __name__=='__main__':main()
