#!/usr/bin/env python3
"""Requires real QEMU/OVMF for Intel/no-IOMMU. AMD optional only if installed QEMU supports it."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from iommu_810_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 a=argparse.ArgumentParser();a.add_argument('--output',required=True,type=Path);o=a.parse_args().output;o.mkdir(parents=True,exist_ok=True)
 build=o/'build'
 with (o/'compile.log').open('w') as f:subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64','--test-iommu-810','--output',str(build)],stdout=f,stderr=subprocess.STDOUT,check=True)
 qemu=shutil.which('qemu-system-x86_64');code=firmware_path('OVMF_CODE','OVMF_CODE');varsfile=firmware_path('OVMF_VARS','OVMF_VARS')
 if not(qemu and code and varsfile):raise RuntimeError('S810 mandatory real QEMU/OVMF unavailable')
 info=subprocess.run([qemu,'--version'],capture_output=True,text=True,check=True).stdout
 (o/'qemu-version.txt').write_text(info)
 helpout=subprocess.run([qemu,'-device','help'],capture_output=True,text=True,check=True)
 (o/'qemu-device-help.log').write_text(helpout.stdout+'\n'+helpout.stderr)
 amd_supported='amd-iommu' in helpout.stdout+helpout.stderr
 modes={}
 scenarios=[('disabled',[]),('intel',['-device','intel-iommu,intremap=on'])]
 if amd_supported:scenarios.append(('amd',['-device','amd-iommu']))
 for mode,options in scenarios:
  dst=o/mode;dst.mkdir(exist_ok=True);shutil.copyfile(varsfile,dst/'OVMF_VARS.fd');serial=dst/'serial.log'
  cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M','-display','none','-vga','std',
   '-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot',
   '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
   '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
   '-drive',f'if=pflash,format=raw,file={dst/"OVMF_VARS.fd"}',
   '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
   '-device','virtio-blk-pci,drive=styleos',*options]
  try:
   r=subprocess.run(cmd,capture_output=True,timeout=115);rc=r.returncode;(dst/'qemu.log').write_bytes(r.stdout+r.stderr)
  except subprocess.TimeoutExpired as e:
   rc=None;(dst/'qemu.log').write_text('TIMEOUT '+str(e))
  raw=serial.read_bytes() if serial.exists() else b''
  good,obs=verify(raw,rc,mode)
  modes[mode]={'status':'PASS' if good else 'FAIL','qemu_exit_code':rc,
               'uart_sha256':hashlib.sha256(raw).hexdigest(),'observations':obs}
  print('S810 QEMU',mode,modes[mode],flush=True)
 if not amd_supported:modes['amd']={'status':'SKIPPED','reason':'qemu -device help lacks amd-iommu; AMD hardware not verified'}
 status='PASS' if all(modes[k]['status']=='PASS' for k in ('disabled','intel')) and modes['amd']['status']!='FAIL' else 'FAIL'
 result={'stage':810,'status':status,'qemu_version':info.splitlines()[0],
  'qemu_amd_iommu_supported':amd_supported,'amd_hardware_verified':modes['amd']['status']=='PASS',
  'qemu_modes':modes,'pci_requesters_verified':False,'iommu_domains_verified':False,
  'dma_isolation_verified':False,'dma_access_authorized':False,'uefi_runtime_verified':False,
  'production_cr3_authorized':False,'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
 (o/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 if status!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
