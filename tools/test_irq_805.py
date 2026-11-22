#!/usr/bin/env python3
"""Mandatory read-only Local-APIC/IDTR/PIC hardware test in real QEMU/OVMF."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from irq_805_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
 out=parser.parse_args().output;out.mkdir(parents=True,exist_ok=True)
 build=out/'build'
 with (out/'compile.log').open('w') as f:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-irq-apic-805','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');nvram=firmware_path('OVMF_VARS','OVMF_VARS')
 if not qemu or not code or not nvram:raise RuntimeError('S805 requires QEMU and OVMF; no simulated fallback')
 vars_file=out/'OVMF_VARS.fd';shutil.copyfile(nvram,vars_file)
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
  result=subprocess.run(cmd,capture_output=True,timeout=90)
  rc=result.returncode;(out/'qemu.log').write_bytes(result.stdout+result.stderr)
 except subprocess.TimeoutExpired as e:(out/'qemu.log').write_text('TIMEOUT '+str(e))
 uart=serial.read_bytes() if serial.exists() else b''
 ok,detail=verify(uart,rc)
 result={'stage':805,'status':'PASS' if ok else 'FAIL','qemu_exit_code':rc,
         'hardware_snapshot_verified':bool(ok),'irq_delivery_verified':False,
         'madt_verified':False,'ioapic_routing_verified':False,
         'dma_iommu_verified':False,'uefi_runtime_verified':False,
         'production_cr3_authorized':False,
         'uart_sha256':hashlib.sha256(uart).hexdigest(),
         'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest(),
         'observations':detail if ok else None,'failure':None if ok else detail}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print('STYLEOS S805 hardware',result['status'],detail,flush=True)
 if not ok:raise SystemExit(1)
if __name__=='__main__':main()
