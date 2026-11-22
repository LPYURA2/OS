#!/usr/bin/env python3
"""Mandatory real Q35/OVMF with and without Intel IOMMU; no synthetic fallback."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from dmar_808_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);out=a.parse_args().output;out.mkdir(parents=True,exist_ok=True)
 build=out/'build'
 with (out/'compile.log').open('w') as f:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-dmar-808','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');varsfile=firmware_path('OVMF_VARS','OVMF_VARS')
 if not (qemu and code and varsfile):raise RuntimeError('S808 requires real QEMU and OVMF')
 modes={}
 for mode,options in [('disabled',[]),('enabled',['-device','intel-iommu,intremap=on'])]:
  where=out/mode;where.mkdir(exist_ok=True);shutil.copyfile(varsfile,where/'OVMF_VARS.fd')
  serial=where/'serial.log'
  cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M','-display','none','-vga','std',
       '-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot',
       '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
       '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
       '-drive',f'if=pflash,format=raw,file={where/"OVMF_VARS.fd"}',
       '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
       '-device','virtio-blk-pci,drive=styleos',*options]
  try:
   r=subprocess.run(cmd,capture_output=True,timeout=100)
   rc=r.returncode;(where/'qemu.log').write_bytes(r.stdout+r.stderr)
  except subprocess.TimeoutExpired as e:
   rc=None;(where/'qemu.log').write_text('TIMEOUT '+str(e))
  raw=serial.read_bytes() if serial.exists() else b''
  good,obs=verify(raw,rc,mode=='enabled')
  modes[mode]={'qemu_exit_code':rc,'status':'PASS' if good else 'FAIL',
               'uart_sha256':hashlib.sha256(raw).hexdigest(),'observations':obs}
  print('S808 real QEMU',mode,modes[mode],flush=True)
 result={'stage':808,'status':'PASS' if all(x['status']=='PASS' for x in modes.values()) else 'FAIL',
  'qemu_modes':modes,'dmar_discovery_verified':all(x['status']=='PASS' for x in modes.values()),
  'dmar_present_with_iommu':modes['enabled']['status']=='PASS',
  'iommu_translation_enabled':False,'dma_isolation_verified':False,
  'dma_access_authorized':False,'uefi_runtime_verified':False,'production_cr3_authorized':False,
  'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 if result['status']!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
