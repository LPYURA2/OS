#!/usr/bin/env python3
"""Independent strict UART evidence parser for terminal 800 hardware observations.

This validates a QEMU trace; an arbitrary self-consistent fixture isn't hardware proof.
The QEMU runner also requires exit=33, exact guest image and captured raw UART.
"""
import re
KEYS={
 'CPU':('cr0','cr3','pat','mtrrcap','mtrrdef','physbits','variable-count'),
 'PCI':('id','gop','bar1raw','bar4raw','bar5raw'),
 'MTRR':('index','base','mask'),
 'TARGET':('index','address','mtrr-type','matches','enabled'),
}
POLICY=b'STYLEOS S800 POLICY physical-mtrr-observed=yes pat-raw-observed=yes effective-pat-pte=unknown irq-apic=unverified dma-iommu=unverified firmware-runtime=unverified production-cr3=deny'
TOKEN=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')

def parse(line,label):
    prefix=b'STYLEOS S800 '+label.encode()+b' '
    if not line.startswith(prefix):raise ValueError('prefix '+label)
    body=line[len(prefix):]
    matches=TOKEN.findall(body)
    if tuple(k.decode() for k,v in matches)!=KEYS[label] or body!=b' '.join(k+b'=0x'+v for k,v in matches):
        raise ValueError('field schema '+label)
    return dict((k.decode(),int(v,16)) for k,v in matches)

def model(addr,bits,cap,default,records):
    if not default&0x800:return (255,0,0)
    winners=[]
    phys=((1<<bits)-1)&~4095
    for item in records:
        base,mask=item['base'],item['mask']
        if not mask&0x800:continue
        if (addr&(mask&phys))==(base&(mask&phys)):
            winners.append(base&255)
    if not winners:return (default&255,0,1)
    if 0 in winners:kind=0
    elif len(set(winners))==1:kind=winners[0]
    elif set(winners)=={4,6}:kind=4
    else:kind=255
    return kind,len(winners),1

def verify(raw,rc):
    try:
        if rc!=33 or not isinstance(raw,bytes) or len(raw)>1000000:raise ValueError('real QEMU debug exit required')
        lines=raw.splitlines()
        if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1 or lines.count(b'STYLEOS S800 PASS')!=1 or lines.count(POLICY)!=1:
            raise ValueError('missing/duplicated marker')
        entries=[line for line in lines if line.startswith(b'STYLEOS S800 ')]
        if len(entries)<6 or not entries[0].startswith(b'STYLEOS S800 CPU ') or not entries[1].startswith(b'STYLEOS S800 PCI '):
            raise ValueError('sequence')
        cpu=parse(entries[0],'CPU'); pci=parse(entries[1],'PCI')
        num=cpu['variable-count']
        if not 1<=num<=16 or num!=(cpu['mtrrcap']&255) or len(entries)!=2+num+3+2:
            raise ValueError('count')
        if not (cpu['cr0']&(1<<31) and cpu['cr3'] and 36<=cpu['physbits']<=52 and
          cpu['pat'] and cpu['mtrrdef']&0xc00 and (cpu['mtrrdef']&~0xcff)==0):
            raise ValueError('cpu')
        allowed_pat={0,1,4,5,6,7}
        if any(((cpu['pat']>>(i*8))&255) not in allowed_pat for i in range(8)):
            raise ValueError('PAT encoding')
        if (pci['id']!=0x10431af4 or not (pci['gop'] and pci['gop']%4096==0) or
            (pci['bar1raw']&15) or (pci['bar4raw']&15)!=12):
            raise ValueError('pci physical addresses')
        regs=[parse(x,'MTRR') for x in entries[2:2+num]]
        if [r['index'] for r in regs]!=list(range(num)):
            raise ValueError('MTRR order')
        if any((r['base']&255) not in (0,1,4,5,6) for r in regs):
            raise ValueError('MTRR type')
        addrs=[pci['gop'],pci['bar1raw']&~15,((pci['bar5raw']<<32)|(pci['bar4raw']&~15))]
        if not all(0<a<1<<cpu['physbits'] for a in addrs) or len(set(addrs))!=3:
            raise ValueError('physical bounds')
        samples=[parse(x,'TARGET') for x in entries[2+num:5+num]]
        for i,row in enumerate(samples):
            expected=model(addrs[i],cpu['physbits'],cpu['mtrrcap'],cpu['mtrrdef'],regs)
            if (row['index'],row['address'],row['mtrr-type'],row['matches'],row['enabled'])!=(i,addrs[i],*expected):
                raise ValueError('MTRR domain mismatch')
        if entries[-2:]!=[POLICY,b'STYLEOS S800 PASS']:
            raise ValueError('policy/terminal')
        if lines.index(b'STYLEOS x86_64 ExitBootServices OK')>=lines.index(entries[0]):
            raise ValueError('boot ordering')
        return True,{'cpu':cpu,'pci':pci,'variable_mtrrs':regs,'targets':samples,
                     'effective_pat_pte_verified':False,'production_cr3_authorized':False}
    except (ValueError,IndexError,KeyError,TypeError) as e:
        return False,str(e)
