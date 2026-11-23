"""S817 independent fail-closed classification. A replay is NOT an IOMMU trust gate."""
from __future__ import annotations
import hashlib
import re
MODES = ('disabled', 'intel', 'bridge-intel', 'bridge-two-hop-intel', 'amd')
MAX_TABLE = 24576
EXPECTED = {'X817':0x817, 'S817':0x5a5}

def validate_table(blob: bytes, signature: bytes) -> bool:
    return (isinstance(blob, bytes) and 36 <= len(blob) <= MAX_TABLE and
            blob[:4] == signature and int.from_bytes(blob[4:8], 'little') == len(blob)
            and sum(blob) % 256 == 0)

def classify_hardware(mode: str, exit_code: int | None, raw: bytes,
                      acpica_log: str, expected_hash: str, *, acpica_returncode: int | None) -> dict:
    good = (mode in MODES and exit_code == 33 and isinstance(raw,bytes) and
            0 < len(raw) < 1_050_000 and hashlib.sha256(raw).hexdigest() == expected_hash
            and acpica_returncode == 0 and 0 < len(acpica_log) <= 1_000_000)
    if not good:
        return {'status':'FAIL','replay_executed':False, 'dma_access_authorized':False,
                'acpi_crs_evaluated_as_hardware':False, 'operation_region_trusted':False}
    return {'status':'REPLAY_ONLY','replay_executed':True,
            'acpi_crs_evaluated_as_hardware':False,
            'acpi_seg_evaluated_as_hardware':False,
            'acpi_bbn_evaluated_as_hardware':False,
            'hostbridge_crs_verified':False,'operation_region_trusted':False,
            'pci_segment_complete':False,'iommu_ownership_complete':False,
            'dma_domains_verified':False,'dma_access_authorized':False,
            'dma_isolation_verified':False,'uefi_runtime_verified':False,
            'production_cr3_authorized':False}

def synthetic_evaluation_ok(log: str) -> bool:
    if not isinstance(log, str) or len(log)>500000: return False
    norm=log.lower()
    for name,value in EXPECTED.items():
        # ACPICA 20230628 prints 16 hex digits without a 0x prefix on the
        # next line. Bind the value to its method, not arbitrary log text.
        pattern=(r'evaluation of[^\n]*'+name.lower()+
                 r'[^\n]*returned object[^\n]*\n\s*\[integer\]\s*=\s*0*'+
                 format(value,'x')+r'\b')
        if not re.search(pattern,norm):return False
    return True

def check_results(result:dict)->bool:
    if not isinstance(result,dict) or result.get('stage')!=817 or result.get('status')!='PASS':return False
    if not result.get('acpica_synthetic_methods_verified') or not result.get('replay_uses_acpiexec'):return False
    for key in ('guest_acpica_integrated','acpi_crs_hardware_verified','acpi_seg_hardware_verified',
                'acpi_bbn_hardware_verified','ecam_reservation_verified', 'pci_segments_complete',
                'iommu_ownership_complete','dma_domains_verified','dma_access_authorized',
                'dma_isolation_verified','uefi_runtime_verified','production_cr3_authorized'):
        if result.get(key) is not False:return False
    modes=result.get('modes',{})
    for key in MODES[:-1]:
        m=modes.get(key,{})
        if m.get('status')!='REPLAY_ONLY' or m.get('qemu_exit_code')!=33:return False
        if m.get('acpi_crs_evaluated_as_hardware') is not False or m.get('dma_access_authorized') is not False:return False
    a=modes.get('amd',{})
    if a.get('status') not in ('REPLAY_ONLY','SKIPPED'):return False
    return True
