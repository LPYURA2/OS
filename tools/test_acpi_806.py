#!/usr/bin/env python3
"""Mandatory hardware ACPI MADT / IOAPIC topology test in real QEMU/OVMF."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from acpi_806_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',required=True,type=Path);out=ap.parse_args().output
 out.mkdir(parents=True,exist_ok=True);build=out/'build'
 with (out/'compile.log').open('w') as f:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-acpi-madt-806','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');nvram=firmware_path('OVMF_VARS','OVMF_VARS')
 if not (qemu and code and nvram):raise RuntimeError('S806 needs real QEMU and OVMF; no stub fallback')
 vars_file=out/'OVMF_VARS.fd';shutil.copyfile(nvram,vars_file)
 uart_path=out/'serial.log'
 cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M',
      '-display','none','-vga','std','-serial',f'file:{uart_path}','-monitor','none','-net','none','-no-reboot',
      '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
      '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
      '-drive',f'if=pflash,format=raw,file={vars_file}',
      '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
      '-device','virtio-blk-pci,drive=styleos']
 rc=None
 try:
  r=subprocess.run(cmd,capture_output=True,timeout=90);rc=r.returncode;(out/'qemu.log').write_bytes(r.stdout+r.stderr)
 except subprocess.TimeoutExpired as e:(out/'qemu.log').write_text('TIMEOUT '+str(e))
 uart=uart_path.read_bytes() if uart_path.exists() else b''
 ok,ob=verify(uart,rc)
 result={'stage':806,'status':'PASS' if ok else 'FAIL','qemu_exit_code':rc,
         'madt_topology_observed':bool(ok),'acpi_checksums_verified':bool(ok),
         'ioapic_redirection_verified':False,'irq_delivery_verified':False,
         'dma_iommu_verified':False,'uefi_runtime_verified':False,'production_cr3_authorized':False,
         'uart_sha256':hashlib.sha256(uart).hexdigest(),
         'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest(),
         'observations':ob if ok else None,'failure':None if ok else ob}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print('STYLEOS S806 hardware',result['status'],ob,flush=True)
 if not ok:raise SystemExit(1)
if __name__=='__main__':main()
