#!/usr/bin/env python3
"""Actual QEMU/OVMF PCI read-only traversal: default and routed PCIe endpoint."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from pci_topology_796_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path)
    p.add_argument('--bridge',action='store_true')
    a=p.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=True)
    target=out/'build'
    with (out/'compile.log').open('w') as f:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
            '--test-pci-topology-796','--output',str(target)],stdout=f,stderr=subprocess.STDOUT,check=True)
    qemu=shutil.which('qemu-system-x86_64')
    code=firmware_path('OVMF_CODE','OVMF_CODE');vars_src=firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not vars_src:
        raise RuntimeError('mandatory QEMU + OVMF not available: never SKIP')
    vars_file=out/'OVMF_VARS.fd';shutil.copyfile(vars_src,vars_file)
    serial=out/'serial.log'
    cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M',
         '-display','none','-vga','std','-serial',f'file:{serial}',
         '-monitor','none','-net','none','-no-reboot',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={vars_file}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={target/"x86_64"/"styleos.img"}',
         '-device','virtio-blk-pci,drive=styleos']
    if a.bridge:
        cmd+=['-device','pcie-root-port,id=rp796,bus=pcie.0,addr=0x1c.0,chassis=1,slot=1',
              '-device','virtio-serial-pci,bus=rp796']
    rc=None
    try:
        run=subprocess.run(cmd,capture_output=True,timeout=90)
        rc=run.returncode;(out/'qemu.log').write_bytes(run.stdout+run.stderr)
    except subprocess.TimeoutExpired as e:
        (out/'qemu.log').write_text('TIMEOUT '+repr(e))
    raw=serial.read_bytes() if serial.exists() else b''
    valid,reason=verify(raw,rc,require_bridge=a.bridge)
    result={'stage':796,'status':'PASS' if valid else 'FAIL',
            'scenario':'bridge' if a.bridge else 'default',
            'qemu_exit_code':rc,'bridge_enumeration_verified':a.bridge and valid,
            'pci_configuration_read_only':True,'unknown_bar_sizes_remain':True,
            'effective_cache_type_verified':False,'irq_routing_verified':False,
            'runtime_services_policy_verified':False,'production_cr3_authorized':False,
            'observations':reason if valid else None,'failure':None if valid else reason,
            'uart_sha256':hashlib.sha256(raw).hexdigest(),
            'efi_sha256':hashlib.sha256((target/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS S796 HARDWARE',result['scenario'],result['status'],reason,flush=True)
    if not valid:raise SystemExit(1)
if __name__=='__main__':main()
