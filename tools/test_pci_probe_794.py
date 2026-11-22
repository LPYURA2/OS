#!/usr/bin/env python3
"""Real QEMU/OVMF bus-zero PCI config-space BAR observation, not a synthetic pass."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from pci_probe_794_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]

def main():
    arg=argparse.ArgumentParser();arg.add_argument('--output',required=True,type=Path)
    out=arg.parse_args().output;out.mkdir(parents=True,exist_ok=True)
    build=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
            '--test-pci-probe-794','--output',str(build)],
            stdout=f,stderr=subprocess.STDOUT,check=True)
    qemu=shutil.which('qemu-system-x86_64')
    code,vars_source=(firmware_path('OVMF_CODE','OVMF_CODE'),
                      firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not code or not vars_source:
        raise RuntimeError('794 real QEMU + OVMF required; never skip this test')
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
    rc=None
    try:
        p=subprocess.run(cmd,capture_output=True,timeout=90)
        rc=p.returncode;(out/'qemu.log').write_bytes(p.stdout+p.stderr)
    except subprocess.TimeoutExpired as e:
        (out/'qemu.log').write_text('TIMEOUT '+repr(e))
    raw=serial.read_bytes() if serial.exists() else b''
    valid,reason=verify(raw,rc)
    result={'stage':794,'status':'PASS' if valid else 'FAIL',
        'qemu_exit_code':rc,'pci_bus_zero_config_read_observed':valid,
        'framebuffer_bar_base_correlated':valid,
        'bar_aperture_size_verified':False,'all_pci_bars_enumerated':False,
        'effective_cache_type_verified':False,'irq_routing_verified':False,
        'runtime_services_policy_verified':False,'production_cr3_authorized':False,
        'observations':reason if valid else None,'failure':None if valid else reason,
        'uart_sha256':hashlib.sha256(raw).hexdigest(),
        'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS S794 HARDWARE',result['status'],reason,flush=True)
    if not valid:raise SystemExit(1)
if __name__=='__main__':main()
