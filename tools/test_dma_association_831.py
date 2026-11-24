"""S828 authentic UART + synthetic MADT; never new hardware or accepted S830."""
import sys
from pathlib import Path
from dma_association_831 import association
from irq_sources_830_verify import POLICY
from test_irq_sources_830_uart import madt
from iommu_evidence_828_verify import BACKEND

def main(root):
    n=0; out={}
    for mode in BACKEND:
        if mode=='amd' and not (root/mode/'serial.log').exists():continue
        raw=(root/mode/'serial.log').read_bytes().replace(b'\r',b'')
        anchor=b'STYLEOS S824 MCFG data=';assert raw.count(anchor)==1
        rec=('STYLEOS S830 MADT data='+madt().hex()+'\n'+POLICY+'\n').encode()
        sample=raw.replace(anchor,rec+anchor)
        r=association(sample,33,mode)
        assert not r['retained_blockers'] and not r['remaining_unclassified']
        assert all(v['state']==('NO_CLAIM' if mode=='disabled' else 'ASSOCIATED') for v in r['requesters'])
        assert all(not v[k] for v in r['requesters'] for k in ('ownership','mmio_authorized','dma_authorized','interrupt_remapping_authorized'))
        assert not any(r[k] for k in ('ownership','mmio_authorized','dma_authorized','interrupt_remapping_authorized','production_cr3_authorized','foundation_complete'))
        assert r['removed_diagnostic_blockers']==([] if mode=='disabled' else ['unresolved-records'])
        out[mode]=r;n+=1
        bad=[(raw,33),(sample.replace(rec,rec+rec),33),(sample.replace(POLICY.encode(),POLICY.replace('dma-grants=no','dma-grants=yes').encode()),33),(sample,0),(sample,32),(sample,None),(sample+b'\0',33)]
        if mode!='disabled':bad.append((sample.replace(madt().hex().encode(),madt(1).hex().encode()),33))
        for forged,rc in bad:
            try:association(forged,rc,mode)
            except ValueError:n+=1
            else:raise AssertionError('accepted forged evidence')
    print(f'{n}/{n} preflight checks PASS: S828 UART with synthetic MADT only')
if __name__=='__main__':main(Path(sys.argv[1]))
