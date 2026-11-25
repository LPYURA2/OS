"""Linked PE evidence for the explicit MS x64 IRQ pointer ABI."""
import re,struct
from fault_site import _range,_unpack

def publication_site(data,map_text):
    pe,=_unpack('<I',data,60,'PE pointer')
    if data[:2]!=b'MZ' or _range(data,pe,4,'PE')!=b'PE\0\0':raise ValueError('PE magic')
    machine,n,_,_,_,opt_size,_=_unpack('<HHIIIHH',data,pe+4,'COFF')
    opt=_range(data,pe+24,opt_size,'optional')
    if machine!=0x8664 or not 0<n<=96 or opt_size<112 or _unpack('<H',opt,0,'magic')[0]!=0x20b:raise ValueError('PE format')
    base,=_unpack('<Q',opt,24,'base');size,headers=_unpack('<II',opt,56,'sizes');start=pe+24+opt_size
    if re.findall(r'^\s*Preferred load address is ([0-9a-fA-F]{16})\s*$',map_text,re.M)!=[f'{base:016x}'] or headers<start+40*n or headers>len(data):raise ValueError('map base')
    def symbol(name,code,span):
        rows=[l for l in map_text.splitlines() if name in l.split()]
        if len(rows)!=1:raise ValueError('symbol count '+name)
        m=re.fullmatch(r'\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+boot_x86_64_irq_delivery_807\.o\s*',rows[0])
        if not m:raise ValueError('symbol row '+name)
        seg,rel,absolute=(int(v,16) for v in m.groups());rva=absolute-base
        if not 1<=seg<=n or not 0<rva<size:raise ValueError('symbol bounds')
        sec,vs,va,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',data,start+40*(seg-1),'section')
        if rva!=va+rel or rel+span>vs or rva+span>size:raise ValueError('section extent')
        if code:
            if sec.rstrip(b'\0')!=b'.text' or not flags&0x20000000 or flags&0x80000000 or rel+span>rs or ro<headers:raise ValueError('code rights')
            return rva,_range(data,ro+rel,span,'code'),ro+rel
        if not flags&0x80000000 or flags&0x20000000 or rva&7:raise ValueError('storage rights')
        return rva,None,None
    ptr,_,_=symbol('irq807_lapic_ptr',False,8);seen,_,_=symbol('irq859_handler_lapic',False,8)
    pub,b,off=symbol('irq859_publish_lapic',True,15)
    def target(pc,ins,disp):return pc+len(ins)+struct.unpack_from('<i',ins,disp)[0]
    if b[:3]!=b'\x48\x89\x0d' or b[7:10]!=b'\x48\x8b\x05' or b[14]!=0xc3 or target(pub,b[:7],3)!=ptr or target(pub+7,b[7:14],3)!=ptr:raise ValueError('publication store/load/return')
    h,bh,hoff=symbol('irq807_handler',True,0x56+6);bad,_,_=symbol('irq807_unexpected',True,1)
    # Exact two saved registers and expected frame captures precede this sequence.
    off_load=0x4e;seq=bh[off_load:off_load+0x12]
    if seq[:3]!=b'\x48\x8b\x05' or target(h+off_load,seq[:7],3)!=ptr or seq[7:10]!=b'\x48\x89\x05' or target(h+off_load+7,seq[7:14],3)!=seen:raise ValueError('handler pointer/observation')
    # Read complete guard independently of the prefix length.
    h,full,hoff=symbol('irq807_handler',True,0x6c)
    guard=full[0x5c:0x66]
    if guard[:8]!=b'\xba\x00\x00\xe0\xfe\x48\x39\xd0' or guard[8]!=0x75 or h+0x66+struct.unpack('b',guard[9:10])[0]!=bad or full[0x66:0x6c]!=b'\x8b\x90\x20\x01\x00\x00':raise ValueError('guard before MMIO')
    if ptr==seen:raise ValueError('aliased observation')
    return {'publish_rva':pub,'lapic_slot_rva':ptr,'handler_lapic_rva':seen,'handler_rva':h,'publish_offset':off,'guard_offset':hoff+0x5c,'handler_load_offset':hoff+off_load}


def mutation_test(data,map_text):
    site=publication_site(data,map_text);count=0
    for key,length in (('publish_offset',15),('handler_load_offset',14),('guard_offset',16)):
        for offset in range(site[key],site[key]+length):
            for bit in range(8):
                changed=bytearray(data);changed[offset]^=1<<bit
                try:publication_site(changed,map_text)
                except ValueError:count+=1;continue
                raise AssertionError('accepted instruction mutation '+str((key,offset,bit)))
    return count
