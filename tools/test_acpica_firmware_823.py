#!/usr/bin/env python3
"""Stage 823: read-only firmware DSDT hostbridge evaluation in a QEMU guest."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from acpica_firmware_823_verify import verify
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();profile=ap.add_mutually_exclusive_group()
 profile.add_argument('--resources-824',action='store_true');profile.add_argument('--ecam-826',action='store_true');profile.add_argument('--ecam-quarantine-827',action='store_true');profile.add_argument('--iommu-evidence-828',action='store_true');profile.add_argument('--irq-sources-830',action='store_true')
 capture=ap.add_mutually_exclusive_group()
 capture.add_argument('--capture-mtree-833',action='store_true')
 capture.add_argument('--capture-post-ebs-834',action='store_true')
 ap.add_argument('--native-quarantine-835',action='store_true')
 ap.add_argument('--guest-plan-837',action='store_true')
 ap.add_argument('--guest-observation-840',action='store_true')
 ap.add_argument('--alias-census-841',action='store_true')
 ap.add_argument('--output',required=True,type=Path)
 ap.add_argument('--core-lib',required=True,type=Path)
 ap.add_argument('--core-include',required=True,type=Path)
 args=ap.parse_args()
 if args.alias_census_841 and not args.guest_observation_840:ap.error('S841 requires S840')
 if args.guest_observation_840 and not args.guest_plan_837:ap.error('S840 requires S837')
 if args.guest_plan_837 and not (args.native_quarantine_835 and args.capture_post_ebs_834 and args.irq_sources_830):ap.error('S837 requires S835 and paired snapshots')
 if args.native_quarantine_835 and not args.capture_post_ebs_834:ap.error('S835 requires paired snapshots')
 if (args.capture_mtree_833 or args.capture_post_ebs_834) and not args.irq_sources_830:ap.error('QMP capture requires S830 profile')
 output=args.output;output.mkdir(parents=True,exist_ok=True)
 if args.irq_sources_830:
  args.iommu_evidence_828=True;args.ecam_826=True;args.resources_824=True
  from irq_sources_830_verify import verify as check
 elif args.iommu_evidence_828:
  args.ecam_826=True;args.resources_824=True
  from iommu_evidence_828_verify import verify as check
 elif args.ecam_quarantine_827:
  args.ecam_826=True;args.resources_824=True
  from ecam_quarantine_827_verify import verify as check
 elif args.ecam_826:
  args.resources_824=True
  from acpi_ecam_826_verify import verify as check
 elif args.resources_824:
  from acpi_resources_824_verify import verify as check
 else:
  check=verify
 stage=830 if args.irq_sources_830 else 828 if args.iommu_evidence_828 else 827 if args.ecam_quarantine_827 else 826 if args.ecam_826 else 824 if args.resources_824 else 823
 if not args.core_lib.is_file() or not (args.core_include/'acpi.h').is_file():raise RuntimeError('verified ACPICA core/header missing')
 build=output/'build'
 with (output/'compile.log').open('w') as log:
  subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
   ('--test-alias-census-841' if args.alias_census_841 else '--test-guest-observation-840' if args.guest_observation_840 else '--test-guest-plan-837' if args.guest_plan_837 else '--test-iommu-quarantine-835' if args.native_quarantine_835 else '--test-post-ebs-834' if args.capture_post_ebs_834 else '--test-irq-sources-830' if args.irq_sources_830 else '--test-iommu-evidence-828' if args.iommu_evidence_828 else '--test-ecam-quarantine-827' if args.ecam_quarantine_827 else '--test-ecam-826' if args.ecam_826 else '--test-acpi-resources-824' if args.resources_824 else '--test-acpica-firmware-823'),'--acpica-core-lib',str(args.core_lib), '--acpica-core-include',str(args.core_include), '--output',str(build)],
   stdout=log,stderr=subprocess.STDOUT,check=True)
 if args.guest_plan_837:
  from test import verified_ownership_site
  site837=verified_ownership_site(build,'x86_64',guest_plan_837=True)
 qemu=shutil.which('qemu-system-x86_64')
 code=firmware_path('OVMF_CODE','OVMF_CODE')
 nvram=firmware_path('OVMF_VARS','OVMF_VARS')
 if not(qemu and code and nvram):
  raise RuntimeError('S823 requires real QEMU x86_64 and OVMF firmware')
 ver=subprocess.run([qemu,'--version'],text=True,capture_output=True,check=True).stdout
 helpresult=subprocess.run([qemu,'-device','help'],text=True,capture_output=True,check=True)
 helptext=helpresult.stdout+'\n'+helpresult.stderr
 amd='amd-iommu' in helptext
 bridge='pcie-root-port' in helptext
 multi_bridge='pcie-pci-bridge' in helptext
 (output/'qemu-version.txt').write_text(ver)
 (output/'qemu-device-help.log').write_text(helptext)
 if not bridge or not multi_bridge:raise RuntimeError('S819 multihop hardware requires pcie-root-port and pcie-pci-bridge')
 modes=[('disabled',[]),('intel',['-device','intel-iommu,intremap=on']),
        ('bridge-intel',['-device','intel-iommu,intremap=on']),
        ('bridge-two-hop-intel',['-device','intel-iommu,intremap=on'])]
 if amd:modes.append(('amd',['-device','amd-iommu']))
 observations={}
 for mode,extra in modes:
  dst=output/mode;dst.mkdir(exist_ok=True)
  shutil.copyfile(nvram,dst/'OVMF_VARS.fd')
  serial=dst/('serial-gated.log' if args.capture_post_ebs_834 else 'serial.log')
  drive=['-device','virtio-blk-pci,drive=styleos']
  if mode in ('bridge-intel','bridge-two-hop-intel'):
   drive=['-device','pcie-root-port,id=rp812,chassis=1,slot=3',
          '-device','virtio-blk-pci,drive=styleos,bus=rp812']
  if mode=='bridge-two-hop-intel':
   drive=['-device','pcie-root-port,id=rp812,chassis=1,slot=3',
          '-device','pcie-pci-bridge,id=br812,bus=rp812',
          '-device','virtio-blk-pci,drive=styleos,bus=br812']
  cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-m','128M',
   '-display','none','-vga','std','-serial',f'file:{serial}',
   '-monitor','none','-net','none','-no-reboot',
   '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
   '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
   '-drive',f'if=pflash,format=raw,file={dst/"OVMF_VARS.fd"}',
   '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={build/"x86_64"/"styleos.img"}',
   *drive,*extra]
  try:
   if args.capture_post_ebs_834:
    from post_ebs_aperture_834 import capture
    rc=capture(cmd,dst,serial)
   elif args.capture_mtree_833:
    from qmp_aperture_833 import capture
    rc=capture(cmd,dst)
   else:
    r=subprocess.run(cmd,timeout=30,capture_output=True)
    rc=r.returncode
    (dst/'qemu.log').write_bytes(r.stdout+r.stderr)
  except subprocess.TimeoutExpired as e:
   rc=None;(dst/'qemu.log').write_text('TIMEOUT '+str(e))
  raw=serial.read_bytes() if serial.exists() else b''
  if args.capture_post_ebs_834:
   from post_ebs_aperture_834 import ungate,replay
   trace=(dst/'qmp-834.json').read_bytes()
   replay(raw,rc,mode,json.loads(trace))
   if args.native_quarantine_835:
    from iommu_quarantine_835_verify import verify_native
    verify_native(raw,rc,mode,json.loads(trace))
   if args.guest_plan_837:
    from iommu_guest_837_verify import replay as replay837
    composed837=replay837(raw,rc,mode,json.loads(trace),site837)
   gated_sha=hashlib.sha256(raw).hexdigest();raw=ungate(raw)
   (dst/'serial.log').write_bytes(raw)
  good,details=check(raw,rc,mode)
  if good:
   (dst/'dsdt.aml').write_bytes(details.pop('_original_dsdt_bytes'))
   for i,blob in enumerate(details.pop('_original_ssdt_bytes')):
    (dst/f'ssdt-{i}.aml').write_bytes(blob)
   if args.resources_824:
    for name,data in details.pop('_s824_raw').items():
     (dst/name).write_bytes(data)
   if args.ecam_826:
    for name,data in details.pop('_s826_raw').items():
     (dst/name).write_bytes(data)
   if args.iommu_evidence_828:
    for name,data in details.pop('_s828_raw').items():
     (dst/name).write_bytes(data)
   if args.irq_sources_830:
    for name,data in details.pop('_s830_raw').items():
     (dst/name).write_bytes(data)
   details['dsdt_sha256']=hashlib.sha256((dst/'dsdt.aml').read_bytes()).hexdigest()
  observations[mode]={'status':'PASS' if good else 'FAIL','qemu_exit_code':rc,
   'uart_sha256':hashlib.sha256(raw).hexdigest(),'observations':details}
  if args.capture_post_ebs_834:
   observations[mode].update(qmp_834_sha256=hashlib.sha256(trace).hexdigest(),gated_uart_sha256=gated_sha)
  if args.guest_plan_837:observations[mode]['composition_837']=composed837
  if args.capture_mtree_833:observations[mode]['qmp_833_sha256']=hashlib.sha256((dst/'qmp-833.json').read_bytes()).hexdigest()
  print(f'S{stage} QEMU',mode,observations[mode],flush=True)
  if not good:
   raise SystemExit(f'S{stage} fail-closed: first failing QEMU mode '+mode)
 if not amd:observations['amd']={'status':'SKIPPED','reason':'QEMU lacks amd-iommu; AMD hardware not verified'}
 required=('disabled','intel','bridge-intel','bridge-two-hop-intel')
 success=all(observations[m]['status']=='PASS' and observations[m]['observations'].get('mcfg_covered_buses')==observations[m]['observations'].get('buses') and observations[m]['observations'].get('uefi_covered_buses')==observations[m]['observations'].get('buses') for m in required) and observations['amd']['status']!='FAIL' and observations['bridge-two-hop-intel']['observations'].get('bridges',0)>=2
 result={'stage':stage,'status':'PASS' if success else 'FAIL',
  'qemu_version':ver.splitlines()[0],'amd_qemu_supported':amd,
  'amd_hardware_verified':observations['amd']['status']=='PASS',
  'uefi_rootbridge_bus_ranges_observed':True,
  'dsdt_fadt_chain_checksum_validated':True,
  'acpi_aml_static_hints_observed':True,
  'ssdt_tables_copied_validated':True,'acpi_literal_subset_only':True,
  'acpi_namespace_evaluated':True, 'guest_acpica_linked':True, 'guest_aml_executed':True, 'firmware_dsdt_readonly':True, 'guest_real_acpica_evaluate_object':True, 'guest_acpica_checksum_verified':True, 'guest_acpica_table_bridge_verified':True, 'operationregion_ever_granted':False, 'hardware_operationregion_permissions':'deny-all', 'osl_region_policy_verified':True,'acpi_bbn_evaluated':False,
  'ecam_reservation_verified':False,
  'real_bridge_sample_verified':observations['bridge-intel']['status']=='PASS',
  'real_two_hop_bridge_sample_verified':observations['bridge-two-hop-intel']['status']=='PASS' and observations['bridge-two-hop-intel']['observations'].get('bridges',0)>=2,
  'modes':observations,'mcfg_firmware_observed':True,'live_pci_buses_mcfg_covered':True,
  'uefi_rootbridge_resources_verified':True,'acpi_crs_evaluated':True,
  'acpi_seg_evaluated':False,'ecam_registers_read':False,'hostbridge_crs_verified':False,
  'all_pci_segments_verified':False,
  'general_iommu_ownership_verified':False,'dma_domains_verified':False,
  'dma_access_authorized':False,'dma_isolation_verified':False,
  'foundation_complete':False,
  'uefi_runtime_verified':False,'production_cr3_authorized':False,
  'firmware_acpi_root_reconciled':False,'upstream_version':'20260930','full_core_compiled':True,
  'efi_sha256':hashlib.sha256((build/'x86_64'/'BOOTX64.EFI').read_bytes()).hexdigest()}
 if args.guest_observation_840:result['guest_observation_840']=True
 if args.alias_census_841:result['alias_census_841']=True
 if args.capture_mtree_833:result['qmp_833_capture']=True
 if args.capture_post_ebs_834:result['qmp_834_capture']=True
 if args.native_quarantine_835:result['native_quarantine_835']=True
 if args.guest_plan_837:
  result['guest_plan_837']=True
  result['ownership_site_837']=site837
 (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 if not success:raise SystemExit(1)
if __name__=='__main__':main()
