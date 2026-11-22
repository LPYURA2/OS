#!/usr/bin/env python3
"""Mandatory single-boot QEMU/OVMF hardware measurement for two downstream BARs."""
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path
from pci_range_799_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    out=ap.parse_args().output;out.mkdir(parents=True,exist_ok=True)
    build=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
            '--test-pci-range-799','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
    qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');nvram=firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not nvram: raise RuntimeError('QEMU/OVMF required, no SKIP')
    vars_copy=out/'OVMF_VARS.fd';shutil.copyfile(nvram,vars_copy)
    serial=out/'serial.log';cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M',
       '-display','none','-vga','std','-serial',f'file:{serial}', '-monitor','none', '-net','none','-no-reboot',
       '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
       '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
       '-drive',f'if=pflash,format=raw,file={vars_copy}',
       '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
       '-device','virtio-blk-pci,drive=styleos',
       '-device','pcie-root-port,id=rp796,bus=pcie.0,addr=0x1c.0,chassis=1,slot=1',
       '-device','virtio-serial-pci,bus=rp796']
    rc=None
    try:
        done=subprocess.run(cmd,capture_output=True,timeout=90)
        rc=done.returncode;(out/'qemu.log').write_bytes(done.stdout+done.stderr)
    except subprocess.TimeoutExpired as ex:
        (out/'qemu.log').write_text('TIMEOUT '+repr(ex))
    raw=serial.read_bytes() if serial.exists() else b''
    ok,obs=verify(raw,rc)
    result={'stage':799,'status':'PASS' if ok else 'FAIL',
        'source':'same-boot-QEMU-PCIe-two-BAR-mask-probes',
        'qemu_exit_code':rc,'qemu_hardware_measured':bool(ok),
        'downstream_full_ranges_forwarded':bool(ok),'bar_registers_and_command_restored':bool(ok),
        'all_bars_verified':False,'effective_cache_type_verified':False,
        'irq_routing_verified':False,'dma_isolation_verified':False,
        'production_cr3_authorized':False,
        'uart_sha256':hashlib.sha256(raw).hexdigest(),
        'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest(),
        'observations':obs if ok else None,'failure':None if ok else obs}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS S799 HARDWARE',result['status'],obs,flush=True)
    if not ok:raise SystemExit(1)
if __name__=='__main__':main()
