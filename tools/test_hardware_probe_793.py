#!/usr/bin/env python3
"""Run exact 793 hardware QEMU/OVMF GOP + firmware facts; no synthetic pass."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from hardware_probe_793_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    out=args.output;out.mkdir(parents=True,exist_ok=True)
    build=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
                       '--test-hw-probe-793','--output',str(build)],
                       stdout=f,stderr=subprocess.STDOUT,check=True)
    qemu=shutil.which('qemu-system-x86_64')
    code,vars_source=(firmware_path('OVMF_CODE','OVMF_CODE'),
                      firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not code or not vars_source:
        raise RuntimeError('793 requires actual qemu-system-x86_64 + OVMF')
    vars_file=out/'OVMF_VARS.fd';shutil.copyfile(vars_source,vars_file)
    serial=out/'serial.log'
    cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M',
        '-display','none','-vga','std','-serial',f'file:{serial}',
        '-monitor','none','-net','none','-no-reboot',
        '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
        '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
        '-drive',f'if=pflash,format=raw,file={vars_file}',
        '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
        '-device','virtio-blk-pci,drive=styleos']
    try:
        p=subprocess.run(cmd,capture_output=True,timeout=90)
        rc=p.returncode
        (out/'qemu.log').write_bytes(p.stdout+p.stderr)
    except subprocess.TimeoutExpired as error:
        rc=None;(out/'qemu.log').write_text('TIMEOUT '+repr(error))
    raw=serial.read_bytes() if serial.exists() else b''
    ok,reason=verify(raw,rc)
    result={'stage':793,'status':'PASS' if ok else 'FAIL',
        'qemu_exit_code':rc,'qemu_hardware_observed':ok,
        'production_cr3_authorized':False,'effective_pat_mtrr_verified':False,
        'mmio_bars_enumerated':False,'all_irq_routes_verified':False,
        'firmware_runtime_policy_proved':False,
        'observations':reason if ok else None,'failure':None if ok else reason,
        'uart_sha256':hashlib.sha256(raw).hexdigest(),
        'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS 793 QEMU OBSERVATION',result['status'],reason,flush=True)
    if not ok:raise SystemExit(1)
if __name__=='__main__':main()
