"""Original S830 UART replay and negative mutation tests, not new hardware."""
import sys
from pathlib import Path
from iommu_base_page_832 import replay
from iommu_evidence_828_verify import BACKEND
n=0
for mode in BACKEND:
    path=Path(sys.argv[1])/mode/'serial.log'
    if mode=='amd' and not path.exists():continue
    raw=path.read_bytes();r=replay(raw,33,mode)
    assert not any(r[k] for k in ('full_aperture_verified','ownership','mmio_authorized','dma_authorized','production_cr3_authorized'))
    for forged,rc in ((raw,0),(raw,32),(raw,None),(raw+b'\0',33)):
        try:replay(forged,rc,mode)
        except ValueError:n+=1
        else:raise AssertionError('accepted forged UART')
print(f'{n}/{n} negative UART replay cases PASS')
