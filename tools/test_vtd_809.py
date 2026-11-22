#!/usr/bin/env python3
"""Mandatory two genuine QEMU/OVMF runs with fail-closed VT-d registers."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from vtd_809_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
 out=parser.parse_args().output;out.mkdir(parents=True,exist_ok=True)
 build=out/'build'
 with (out/'compile.log').open('w') as f:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-vtd-809','--output',str(build)],check=True,stdout=f,stderr=subprocess.STDOUT)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');varsfile=firmware_path('OVMF_VARS','OVMF_VARS')
 if not (qemu and code and varsfile):raise RuntimeError('S809 requires actual QEMU and OVMF')
 modes={}
 for mode,extra in [('disabled',[]),('enabled',['-device','intel-iommu,intremap=on'])]:
  folder=out/mode;folder.mkdir(exist_ok=True);shutil.copyfile(varsfile,folder/'OVMF_VARS.fd')
  serial=folder/'serial.log'
  cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M','-display','none','-vga','std',
       '-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot',
       '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
       '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
       '-drive',f'if=pflash,format=raw,file={folder/"OVMF_VARS.fd"}',
       '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
       '-device','virtio-blk-pci,drive=styleos',*extra]
  try:
   proc=subprocess.run(cmd,capture_output=True,timeout=100)
   rc=proc.returncode;(folder/'qemu.log').write_bytes(proc.stdout+proc.stderr)
  except subprocess.TimeoutExpired as err:
   rc=None;(folder/'qemu.log').write_text('TIMEOUT: '+str(err))
  raw=serial.read_bytes() if serial.exists() else b''
  ok,obs=verify(raw,rc,mode=='enabled')
  modes[mode]={'qemu_exit_code':rc,'status':'PASS' if ok else 'FAIL',
               'uart_sha256':hashlib.sha256(raw).hexdigest(),'observations':obs}
  print('S809 real QEMU',mode,modes[mode],flush=True)
 result={'stage':809,'status':'PASS' if all(x['status']=='PASS' for x in modes.values()) else 'FAIL',
         'qemu_modes':modes,'iommu_registers_observed':modes['enabled']['status']=='PASS',
         'dma_isolation_verified':False,'dma_access_authorized':False,'iommu_domains_verified':False,
         'uefi_runtime_verified':False,'production_cr3_authorized':False,
         'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 if result['status']!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
