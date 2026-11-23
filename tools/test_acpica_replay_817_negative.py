"""Host-side negative tests for S817 replay evidence. No acpiexec binary needed."""
import hashlib, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from acpica_replay_817_verify import (validate_table, classify_hardware,
                                      synthetic_evaluation_ok, check_results)
checks=0
def expect_false(value):
    global checks
    assert value is False, repr(value)
    checks+=1
blob=bytearray(36)
blob[:4]=b'DSDT';blob[4:8]=(36).to_bytes(4,'little');blob[9]=(-sum(blob))&255
assert validate_table(bytes(blob),b'DSDT')
for sig in (b'FACP',b'SSDT',b'XXXX'):
    expect_false(validate_table(bytes(blob),sig))
for n in (0,10,35,24577):
    expect_false(validate_table(bytes(blob)[:n] if n!=24577 else bytes(blob)+b'\x00'*24541,b'DSDT'))
for pos in (0,4,7,8,9,22,35):
    bad=bytearray(blob);bad[pos]^=1
    expect_false(validate_table(bytes(bad),b'DSDT'))
raw=b'STYLEOS S816 REAL UART\n';sha=hashlib.sha256(raw).hexdigest()
for mode in ('disabled','intel','amd'):
    a=classify_hardware(mode,33,raw,'ACPICA VM replayed',sha,acpica_returncode=0)
    assert a['status']=='REPLAY_ONLY'
    assert not any(v is True for k,v in a.items() if k.endswith('authorized') or k.endswith('verified'))
    checks+=1
    for rc,expected,h,l,ret in ((1,sha,raw,'ACPICA VM replayed',0),
                               (None,sha,raw,'ACPICA VM replayed',0),
                               (33,'0'*64,raw,'ACPICA VM replayed',0),
                               (33,sha,b'bad','ACPICA VM replayed',0),
                               (33,sha,raw,'',0),(33,sha,raw,'ACPICA VM replayed',1)):
        assert classify_hardware(mode,rc,h,l,expected,acpica_returncode=ret)['status']=='FAIL'
        checks+=1
for msg in ('','STYLEOS 817 PASS','X817 0x817 S817 0x5a5',
            'Evaluation of \\_SB.PCI0.X817 returned object 0x123 0x817',
            'Evaluation of \\_SB.PCI0.S817 returned object 0x5a5'):
    expect_false(synthetic_evaluation_ok(msg))
example={'stage':817,'status':'PASS','acpica_synthetic_methods_verified':True,
         'replay_uses_acpiexec':True,
         'modes':{m:{'status':'REPLAY_ONLY','qemu_exit_code':33,
                     'acpi_crs_evaluated_as_hardware':False,
                     'dma_access_authorized':False} for m in ('disabled','intel','bridge-intel','bridge-two-hop-intel','amd')},
         **{k:False for k in ('guest_acpica_integrated','acpi_crs_hardware_verified',
          'acpi_seg_hardware_verified','acpi_bbn_hardware_verified',
          'ecam_reservation_verified','pci_segments_complete','iommu_ownership_complete',
          'dma_domains_verified','dma_access_authorized','dma_isolation_verified',
          'uefi_runtime_verified','production_cr3_authorized')}}
assert check_results(example);checks+=1
for k in ('guest_acpica_integrated','acpi_crs_hardware_verified',
          'ecam_reservation_verified','pci_segments_complete','iommu_ownership_complete',
          'dma_domains_verified','dma_access_authorized','dma_isolation_verified',
          'uefi_runtime_verified','production_cr3_authorized'):
    bad=dict(example);bad[k]=True;expect_false(check_results(bad))
for mode in ('disabled','intel','bridge-intel','bridge-two-hop-intel'):
    bad={**example,'modes':{**example['modes'],mode:{**example['modes'][mode],'qemu_exit_code':0}}}
    expect_false(check_results(bad))
print(f'STYLEOS S817 NEGATIVE PASS {checks}/{checks}')
