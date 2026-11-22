#!/usr/bin/env python3
"""Independent conservative audit of full three cited MMIO extents in ONE QEMU boot.

Checks all 4KiB physical pages analytically from captured MTRRs and each active
PTE leaf record. It is NOT a global alias scan or production cache grant.
"""
from cache_800_verify import TOKEN, model

FIELDS={
 'CPU':('cr0','cr3','pat','mtrrcap','mtrrdef','physbits','variable-count'),
 'PCI':('id','gop','bar1raw','bar4raw','bar5raw'),
 'MTRR':('index','base','mask'),
 'RANGE':('index','base','bytes','end','pages'),
 'SEG':('index','segment','start','end','last-physical','page-size','entry','pat-index','pat-type','mtrr-type','pages'),
}
POLICY=b'STYLEOS S804 POLICY entire-three-known-ranges=yes unknown-mmio=unverified global-alias-scan=unverified irq-apic=unverified dma-iommu=unverified firmware-runtime=unverified production-cr3=deny'
BASES=(0x80000000,0x81000000,0xc000000000)
LENGTHS=(4096000,4096,16384)

def parse(line,kind):
    prefix=b'STYLEOS S804 '+kind.encode()+b' '
    if not line.startswith(prefix):raise ValueError('prefix '+kind)
    body=line[len(prefix):]
    parts=TOKEN.findall(body)
    if tuple(k.decode() for k,v in parts)!=FIELDS[kind] or body!=b' '.join(k+b'=0x'+v for k,v in parts):
        raise ValueError('schema '+kind)
    return {k.decode():int(v,16) for k,v in parts}

def verify(uart,rc):
    try:
        if not isinstance(uart,bytes) or len(uart)>1000000 or rc!=33:
            raise ValueError('QEMU rc 33 and bounded UART required')
        lines=uart.splitlines()
        if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:
            raise ValueError('firmware ExitBootServices marker')
        records=[x for x in lines if x.startswith(b'STYLEOS S804 ')]
        if len(records)<12 or records[-2:]!=[POLICY,b'STYLEOS S804 PASS']:
            raise ValueError('terminal policy/sequence')
        if any(x.startswith(b'STYLEOS S804 FAIL') for x in lines):
            raise ValueError('failure marker')
        cpu=parse(records[0],'CPU');pci=parse(records[1],'PCI')
        n=cpu['variable-count']
        if not (1<=n<=16 and n==cpu['mtrrcap']&255 and cpu['cr3']&~4095 and
                cpu['cr0']&(1<<31) and 36<=cpu['physbits']<=52 and
                cpu['mtrrdef']&0xc00==0xc00 and cpu['mtrrdef']&~0xcff==0):
            raise ValueError('CPU/MTRR capabilities')
        if any(((cpu['pat']>>(i*8))&255) not in (0,1,4,5,6,7) for i in range(8)):
            raise ValueError('invalid PAT encoding')
        if pci['id']!=0x10431af4 or pci['gop']!=BASES[0] or pci['bar1raw']&~15!=BASES[1] or \
           ((pci['bar5raw']<<32)|(pci['bar4raw']&~15))!=BASES[2] or \
           pci['bar1raw']&15 or pci['bar4raw']&15!=12:
            raise ValueError('wrong PCI/device bases')
        regs=[parse(x,'MTRR') for x in records[2:2+n]]
        if [x['index'] for x in regs]!=list(range(n)) or \
           any(x['base']&255 not in (0,1,4,5,6) for x in regs):
            raise ValueError('MTRR values')
        pos=2+n
        validated=[];segments=[];total_pages=0
        for i,(base,length) in enumerate(zip(BASES,LENGTHS)):
            r=parse(records[pos],'RANGE');pos+=1
            if r!={'index':i,'base':base,'bytes':length,'end':base+length,'pages':length//4096}:
                raise ValueError('range length, count or order')
            cursor=base;segment=0;range_pages=0
            while cursor<base+length:
                if pos>=len(records)-2:raise ValueError('missing segment')
                x=parse(records[pos],'SEG');pos+=1
                sz=x['page-size'];e=x['entry']
                if sz not in (4096,2097152,1073741824) or not e&1 or \
                   (sz>4096 and not e&128):raise ValueError('invalid leaf')
                if (x['index'],x['segment'],x['start'])!=(i,segment,cursor):
                    raise ValueError('missing, repeated or misplaced segment')
                stop=min(base+length,(cursor&~(sz-1))+sz)
                if x['end']!=stop or stop<=cursor or x['pages']!=(stop-cursor)//4096:
                    raise ValueError('segment boundary/page count')
                bits=cpu['physbits']
                mask=(((1<<bits)-1)&~4095)&~(sz-1)
                reserved=(sz-1)&~4095
                if sz>4096:reserved&=~4096
                if e&reserved:raise ValueError('reserved leaf flags')
                phys_start=(e&mask)+(cursor&(sz-1))
                phys_end=(e&mask)+((stop-1)&(sz-1))
                idx=(((e>>(7 if sz==4096 else 12))&1)<<2)|(((e>>4)&1)<<1)|((e>>3)&1)
                pat=(cpu['pat']>>(8*idx))&255
                if (phys_start,phys_end,idx,pat)!=(cursor,x['last-physical'],x['pat-index'],x['pat-type']):
                    raise ValueError('wrong physical translation/PAT flags')
                if pat!=6 or x['mtrr-type']!=0:raise ValueError('cache-policy mismatch')
                for addr in range(cursor,stop,4096):
                    kind,hits,enabled=model(addr,bits,cpu['mtrrcap'],cpu['mtrrdef'],regs)
                    if kind!=0 or enabled!=1:raise ValueError('MTRR non-UC range page')
                segments.append(x)
                range_pages+=x['pages'];total_pages+=x['pages'];cursor=stop;segment+=1
                if segment>128:raise ValueError('excess segments')
            if range_pages!=r['pages']:raise ValueError('range incomplete')
            validated.append(r)
        if records[pos:]!=[POLICY,b'STYLEOS S804 PASS']:
            raise ValueError('extra/injected records')
        if lines.index(b'STYLEOS x86_64 ExitBootServices OK')>=lines.index(records[0]):
            raise ValueError('boot order')
        if total_pages!=1005:raise ValueError('total pages incorrect')
        return True,{'cpu':cpu,'pci':pci,'ranges':validated,'segments':segments,
                     'validated_4k_pages':total_pages,'same_boot':True,
                     'global_alias_scan':False,'other_mmio_verified':False,
                     'production_cr3_authorized':False}
    except (ValueError,IndexError,KeyError,TypeError,OverflowError) as e:
        return False,str(e)
