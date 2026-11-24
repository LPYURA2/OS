"""Independent same-boot QMP/UART topology correlation; no device admission."""
import re
POLICY=b'STYLEOS S856 POLICY pci-census=yes ata-access=no data-write=no dma=no admission=no production-cr3=deny'

def verify(raw, root, qmp):
    if not root or root&4095 or any(b' FAIL ' in x for x in raw.splitlines()):raise ValueError('root/fail')
    rows=[x for x in raw.splitlines() if x.startswith(b'STYLEOS S856 ')]
    if len(rows)<3 or rows[-1]!=POLICY or rows.count(POLICY)!=1:raise ValueError('policy')
    def parse(row,tag,keys):
        m=re.fullmatch(('STYLEOS S856 '+tag+' ').encode()+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in ('root',*keys)),row)
        if not m:raise ValueError('schema '+tag)
        v=tuple(int(x,16) for x in m.groups())
        if v[0]!=root:raise ValueError('root')
        return v[1:]
    count,ide_count,cf8a,cf8b=parse(rows[-2],'TOTAL',('functions','ide','cf8-before','cf8-after'))
    if cf8a!=cf8b or cf8a>0xffffffff:raise ValueError('CF8')
    pci={};ide={};previous=-1
    for row in rows[:-2]:
        if row.startswith(b'STYLEOS S856 PCI '):
            bdf,id,cls=parse(row,'PCI',('bdf','id','class'))
            if not previous<bdf<256 or not 0<id<=0xffffffff or id&65535 in (0,65535) or cls>0xffffffff:raise ValueError('PCI')
            pci[bdf]=(id,cls);previous=bdf
        else:
            bdf,*v=parse(row,'IDE',('bdf','command-status','header','timing','bar0','bar1','bar2','bar3','bar4'))
            if bdf!=previous or bdf in ide or pci[bdf][1]>>16!=0x0101 or any(x>0xffffffff for x in v):raise ValueError('IDE')
            ide[bdf]=v
    if len(pci)!=count or len(ide)!=ide_count or set(ide)!={b for b,(_,c) in pci.items() if c>>16==0x0101}:raise ValueError('count/IDE coverage')
    expected={}
    for bus in qmp:
        if bus['bus']!=0:continue
        for d in bus['devices']:
            b=d['slot']*8+d['function']; ident=d['id']; cls=d['class_info']['class']
            if b in expected:raise ValueError('QMP duplicate')
            expected[b]=(ident['device']<<16|ident['vendor'],cls)
    if expected!={b:(i,c>>16) for b,(i,c) in pci.items()}:raise ValueError('QMP mismatch')
    candidates=[]
    for b,v in ide.items():
        status,header,timing,*bars=v
        ident,cls=pci[b]
        # Same narrow policy as previous C admission; this is only a static
        # compatibility observation. No ownership or attachment authorization.
        if ident==0x70108086 and cls>>8==0x010180 and status&7==1 and (header>>16)&127==0 and timing&0x80000000 and all(x in (0,y) for x,y in zip(bars[:4],(0x1f1,0x3f5,0x171,0x375))):candidates.append(b)
    return {'functions':count,'ide':ide_count,'compatible_static_candidates':candidates,'admission':False,'ata_access':False,'pci':pci,'ide_config':ide}
