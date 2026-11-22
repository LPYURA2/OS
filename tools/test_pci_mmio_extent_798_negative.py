#!/usr/bin/env python3
"""Self-contained synthetic refusal tests: NOT additional hardware evidence."""
import hashlib,copy
from pci_aperture_795_verify import POLICY as P795
from pci_topology_796_verify import POLICY as P796
from pci_forwarding_797 import audit as forwarding_audit
from pci_mmio_extent_798 import inspect,ExtentError

def line(stage,tag,values):
    return ('STYLEOS S%d %s '%(stage,tag)).encode()+b' '.join(
        k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in values.items())

g=dict(base=0x80000000,bytes=4096000,width=1280,height=800,**{'uefi-overlaps':0})
p=dict(bus=0,device=1,function=0,vendor=0x1234,id=0x1111,devices=6,index=0,width=32,base=0x80000000)
s=dict(**{'cmd-before':7,'cmd-restored':7,'mask-lo':0xff000008,'mask-hi':0,
          'decode-disabled':1,'registers-restored':1,'size':0x1000000,'end':0x81000000})

def mk795(pp=p,gg=g,ss=s):
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',line(795,'GOP',gg),
       line(795,'PCI',pp),line(795,'SIZING',ss),P795,b'STYLEOS S795 PASS',b''])

def device(bus,idx,cls,hdr):
    return line(796,'DEVICE',dict(bus=bus,device=idx,function=0,vendor=0x1234,
           id=0x1111,**{'class':cls,'subclass':4 if cls==6 else 0,'header':hdr}))

def bar(bus,base,idx=0,prefetch=1):
    return line(796,'BAR',dict(bus=bus,device=1 if bus==0 else 0,function=0,
        index=idx,memory=1,base=base,width=32 if not prefetch else 64,
        prefetch=prefetch))

def mk796(bridge=False, extra_bar=None):
    rows=[device(0,1,3,0),bar(0,0x80000000,0,1)]
    bs=1
    if bridge:
        rows += [device(0,0x1c,6,1),line(796,'BRIDGE',dict(primary=0,secondary=1,
          subordinate=1,**{'memory-base':0x81000000,'memory-end':0x81200000,
            'prefetch-base':0xc000000000,'prefetch-end':0xc020000000})),
          device(1,0,3,0),bar(1,0x81000000,1,0),bar(1,0xc000000000,4,1)]
        bs=2
    if extra_bar is not None:rows.append(bar(0,extra_bar,2,1))
    rows+=[line(796,'INVENTORY',dict(buses=bs,devices=3 if bridge else 1,
       bridges=1 if bridge else 0,routes=1 if bridge else 0,
       **{'bridge-windows':1 if bridge else 0,
       'memory-bars':len([r for r in rows if r.startswith(b'STYLEOS S796 BAR')]),
       'io-bars':0,'bar-sizes-unknown':len([r for r in rows if r.startswith(b'STYLEOS S796 BAR')]),
       'display-matches':1})),line(796,'GOP',dict(base=0x80000000,bytes=4096000,
          bus=0,device=1,function=0,**{'observed-nonzero':1})),P796,b'STYLEOS S796 PASS']
    return b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',*rows,b''])

def pack(ap=None, default=None, bridge=None):
    a=ap if ap is not None else mk795()
    d=default if default is not None else mk796()
    b=bridge if bridge is not None else mk796(True)
    r795={'stage':795,'status':'PASS','qemu_exit_code':33,
      'bar_aperture_size_verified':True,'bar_probe_restored':True,
      'framebuffer_aperture_covered':True,'production_cr3_authorized':False,
      'uart_sha256':hashlib.sha256(a).hexdigest()}
    top={}; tdata={}; fdata={}
    for label,raw,required in [('default',d,False),('bridge',b,True)]:
        x=forwarding_audit(raw,33,require_bridge=required)
        top[label]=raw
        tdata[label]={'stage':796,'status':'PASS','qemu_exit_code':33,
          'scenario':label,'production_cr3_authorized':False,
          'unknown_bar_sizes_remain':True,'uart_sha256':hashlib.sha256(raw).hexdigest()}
        fdata[label]={'serial_sha256':hashlib.sha256(raw).hexdigest(),
          **{k:({str(q):count for q,count in v.items()} if k=='routed_memory_by_bus' else v)
          for k,v in x.items()}}
    f={'stage':797,'status':'PASS','source':'same-run-hardware-S796-QEMU',
       'production_cr3_authorized':False,'observations':fdata}
    return (a,r795,top,tdata,f)

data=pack();out=inspect(*data)
assert out['verified_apertures'][0]['size_bytes']==0x1000000
assert len(out['topologies']['bridge']['resources_without_full_aperture_attestation'])==3
assert out['all_mmio_apertures_verified'] is False
assert out['production_cr3_authorized'] is False
count=0

def reject(arg):
    global count
    try: inspect(*arg)
    except (ValueError,TypeError,KeyError,IndexError):count+=1
    else:raise AssertionError('synthetic unsafe evidence accepted')

for field,value in [('uart_sha256','0'),('qemu_exit_code',0),('status','FAIL'),
                    ('bar_probe_restored',False),('bar_aperture_size_verified',False),
                    ('production_cr3_authorized',True)]:
    a=list(copy.deepcopy(data));a[1][field]=value;reject(a)
for label,field,value in [('default','uart_sha256','0'),('bridge','status','FAIL'),
    ('bridge','unknown_bar_sizes_remain',False),('default','production_cr3_authorized',True),
    ('bridge','qemu_exit_code',0)]:
    a=list(copy.deepcopy(data));a[3][label][field]=value;reject(a)
for field,value in [('status','FAIL'),('production_cr3_authorized',True),
                    ('source','fabricated'),('stage',796)]:
    a=list(copy.deepcopy(data));a[4][field]=value;reject(a)
for field,value in [('serial_sha256','0'),('total_bars',99),
                    ('all_bar_aperture_sizes_verified',True)]:
    a=list(copy.deepcopy(data));a[4]['observations']['bridge'][field]=value;reject(a)
for altered in [mk795(pp=dict(p,base=0x81000000)),
                mk795(ss=dict(s,**{'cmd-restored':0})),
                mk795(gg=dict(g,bytes=0))]:
    try:reject(pack(ap=altered))
    except ValueError:count+=1  # S795 does not admit even a synthetic fixture in preflight
# A second unknown BAR base inside the independently measured VGA range is a
# fail-closed overlap, even if the S796 validator and routing audit both pass.
reject(pack(default=mk796(extra_bar=0x80001000)))
# Different GOP and PCI BAR initial base with otherwise self-consistent logs.
reject(pack(default=mk796().replace(b'0000000080000000',b'0000000090000000')))
assert count>=20,count
print('STYLEOS S798 NEGATIVE PASS',count)
