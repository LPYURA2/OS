#!/usr/bin/env python3
"""Mandatory real hardware PIT/IOAPIC/LAPIC IRQ; no host/stub substitute."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from apic_root_842_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);out=p.parse_args().output
 out.mkdir(parents=True,exist_ok=True);build=out/'build'
 with (out/'compile.log').open('w') as f:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-apic-root-842','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');nvram=firmware_path('OVMF_VARS','OVMF_VARS')
 if not (qemu and code and nvram):raise RuntimeError('S842 requires installed real QEMU/OVMF; no synthetic hardware fallback')
 shutil.copyfile(nvram,out/'OVMF_VARS.fd');serial=out/'serial.log'
 cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M','-display','none','-vga','none',
 '-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot',
 '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
 '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
 '-drive',f'if=pflash,format=raw,file={out/"OVMF_VARS.fd"}',
 '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
 '-device','virtio-blk-pci,drive=styleos']
 rc=None
 try:
  run=subprocess.run(cmd,capture_output=True,timeout=100);rc=run.returncode;(out/'qemu.log').write_bytes(run.stdout+run.stderr)
 except subprocess.TimeoutExpired as e:(out/'qemu.log').write_text('TIMEOUT '+str(e))
 uart=serial.read_bytes() if serial.exists() else b'';ok,obs=verify(uart,rc)
 result={'stage':842,'status':'PASS' if ok else 'FAIL','qemu_exit_code':rc,'ioapic_redirection_verified':bool(ok),
 'owned_root_irq_verified':bool(ok),'irq_delivery_verified':bool(ok),'lapic_eoi_verified':bool(ok),'pit_state_restored':False,
 'dma_iommu_verified':False,'uefi_runtime_verified':False,'production_cr3_authorized':False,
 'uart_sha256':hashlib.sha256(uart).hexdigest(),
 'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest(),
 'observations':obs if ok else None,'failure':None if ok else obs}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print('STYLEOS S842 real QEMU',result['status'],obs,flush=True)
 if not ok:raise SystemExit(1)
if __name__=='__main__':main()
