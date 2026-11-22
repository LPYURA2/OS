#!/usr/bin/env python3
"""Strict SAME-BOOT MTRR/PAT/PTE QEMU UART audit; no global alias proof."""
from cache_800_verify import model
from cache_801_verify import TOKEN
KEYS={
 'CPU':('cr0','cr3','pat','mtrrcap','mtrrdef','physbits','variable-count'),
 'PCI':('id','gop','bar1raw','bar4raw','bar5raw'),
 'MTRR':('index','base','mask'),
 'TARGET':('index','address','mtrr-type','matches','enabled','present','translated',
           'page-size','entry','pat-index','pat-type','effective-sample-type'),
}
POLICY=b'STYLEOS S803 POLICY single-boot-samples=yes global-alias-scan=unverified all-mmio-cache=unverified irq-apic=unverified dma-iommu=unverified firmware-runtime=unverified production-cr3=deny'
ADDRESSES=(0x80000000,0x81000000,0xc000000000)
def parse(line,kind):
    head=b'STYLEOS S803 '+kind.encode()+b' '
    if not line.startswith(head):raise ValueError('prefix')
    body=line[len(head):]
    parts=TOKEN.findall(body)
    if tuple(k.decode() for k,v in parts)!=KEYS[kind] or body!=b' '.join(k+b'=0x'+v for k,v in parts):
        raise ValueError('schema '+kind)
    return {k.decode():int(v,16) for k,v in parts}
def verify(uart,rc):
    try:
        if not isinstance(uart,bytes) or len(uart)>1000000 or rc!=33:raise ValueError('real QEMU exit=33 required')
        lines=uart.splitlines()
        if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:raise ValueError('exit boot')
        records=[x for x in lines if x.startswith(b'STYLEOS S803 ')]
        if len(records)<8 or records[-2:]!=[POLICY,b'STYLEOS S803 PASS']:
            raise ValueError('sequence/terminal')
        cpu=parse(records[0],'CPU');pci=parse(records[1],'PCI')
        count=cpu['variable-count']
        if not 1<=count<=16 or count!=cpu['mtrrcap']&255 or len(records)!=2+count+3+2:
            raise ValueError('variable MTRR count')
        if not (cpu['cr0']&(1<<31) and cpu['cr3']&~4095 and 36<=cpu['physbits']<=52 and
                cpu['mtrrdef']&0xc00==0xc00 and cpu['mtrrdef']&~0xcff==0):
            raise ValueError('CPU/MTRR')
        if any((cpu['pat']>>(8*i))&255 not in (0,1,4,5,6,7) for i in range(8)):
            raise ValueError('PAT types')
        if pci['id']!=0x10431af4 or pci['gop']!=ADDRESSES[0] or pci['bar1raw']&~15!=ADDRESSES[1] or ((pci['bar5raw']<<32)|(pci['bar4raw']&~15))!=ADDRESSES[2] or (pci['bar4raw']&15)!=12 or pci['bar1raw']&15:
            raise ValueError('PCI device/address')
        regs=[parse(x,'MTRR') for x in records[2:2+count]]
        if [r['index'] for r in regs]!=list(range(count)) or any(r['base']&255 not in (0,1,4,5,6) for r in regs):
            raise ValueError('variable MTRR schema')
        targets=[parse(x,'TARGET') for x in records[2+count:5+count]]
        for i,t in enumerate(targets):
            a=ADDRESSES[i]
            if (t['index'],t['address'])!=(i,a):raise ValueError('target/sequence')
            kind,hits,enabled=model(a,cpu['physbits'],cpu['mtrrcap'],cpu['mtrrdef'],regs)
            if (t['mtrr-type'],t['matches'],t['enabled'])!=(kind,hits,enabled) or kind!=0 or enabled!=1:
                raise ValueError('unproven physical UC')
            size=t['page-size'];e=t['entry']
            if t['present']!=1 or size not in (4096,2097152,1073741824) or not e&1:
                raise ValueError('missing PTE')
            if size!=4096 and not(e&128):raise ValueError('page size PS bit')
            bits=cpu['physbits']
            mask=(((1<<bits)-1)&~4095)&~(size-1)
            reserved=(size-1)&~4095
            if size>4096: reserved&=~4096
            if e&reserved:raise ValueError('reserved leaf bits')
            physical=(e&mask)+(a&(size-1))
            idx=(((e>>(7 if size==4096 else 12))&1)<<2)|(((e>>4)&1)<<1)|((e>>3)&1)
            pat_type=(cpu['pat']>>(8*idx))&255
            if (physical,idx,pat_type)!=(t['translated'],t['pat-index'],t['pat-type']) or physical!=a:
                raise ValueError('incorrect active translation/PAT')
            if t['effective-sample-type']!=0:raise ValueError('effective UC dominance')
        if lines.index(b'STYLEOS x86_64 ExitBootServices OK')>=lines.index(records[0]):
            raise ValueError('boot evidence order')
        if any(x.startswith(b'STYLEOS S803 FAIL') for x in lines):raise ValueError('FAIL in UART')
        return True,{'samples':targets,'cpu':cpu,'pci':pci,'same_boot_atomic_measurement':True,
                     'global_mmio_alias_scan_verified':False,'all_mmio_cache_types_verified':False,
                     'production_cr3_authorized':False,'effective_sample_type':'UC'}
    except (ValueError,TypeError,IndexError,KeyError,OverflowError) as e:
        return False,str(e)
