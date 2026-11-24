"""Synthetic MADT in actual S828 UART; not same-boot hardware evidence."""
import sys,struct
from pathlib import Path
from irq_sources_830_verify import verify,POLICY
from iommu_evidence_828_verify import BACKEND

def madt(id=0):
    b=bytearray(44);b[:4]=b'APIC';b[8]=1;b[36:40]=(0xfee00000).to_bytes(4,'little')
    b.extend(struct.pack('<BBBBII',1,12,id,0,0xfec00000,0));b[4:8]=len(b).to_bytes(4,'little');b[9]=(-sum(b))%256
    return bytes(b)

def main(folder):
    count=0
    for mode in BACKEND:
        p=folder/mode/'serial.log'
        if mode=='amd' and not p.exists():continue
        raw=p.read_bytes().replace(b'\r',b'');anchor=b'STYLEOS S824 MCFG data=';assert raw.count(anchor)==1
        rec=('STYLEOS S830 MADT data='+madt().hex()+'\n'+POLICY+'\n').encode()
        good=raw.replace(anchor,rec+anchor);ok,d=verify(good,33,mode)
        assert ok and d['s830_irq_inventory']['same_boot_madt_proven'] and not d['s830_dma_authorized']
        count+=1
        bad=[raw,good.replace(rec,rec+rec),good.replace(madt().hex().encode(),madt().hex()[:-2].encode()+b'01'),good.replace(POLICY.encode(),POLICY.replace('interrupt-remapping=no','interrupt-remapping=yes').encode()),raw+rec,good+b'\0']
        if mode!='disabled':bad.append(good.replace(madt().hex().encode(),madt(1).hex().encode()))
        for sample in bad:assert not verify(sample,33,mode)[0];count+=1
        for rc in (None,0,32,34):assert not verify(good,rc,mode)[0];count+=1
    print(f'S830 {count}/{count} synthetic UART checks PASS; no new hardware')
if __name__=='__main__':main(Path(sys.argv[1]))
