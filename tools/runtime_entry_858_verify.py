"""Validate real S851/S852 evidence plus service bootstrap under owned CR3."""
import re
from desktop_frame_852_verify import verify as prior, POLICY as FRAME
from desktop_switch_851_verify import POLICY as SWITCH
POLICY=b'STYLEOS S858 POLICY runtime-services=yes empty-registry=yes frame=yes irq=no app-admission=no storage=no production-cr3=deny'
KEYS=('root','old','gdt','idt','tr','cs','rsp','cr0','cr4','efer','flags','gdt-limit','idt-limit','safe-root','processes','windows','session-next','selected','ports')
def verify(raw,rc,rva,reference,*,host_quit=False):
    try:
        ok,data=prior(raw,rc,rva,reference,host_quit=host_quit)
        if not ok:raise ValueError(data)
        lines=raw.splitlines();records=[x for x in lines if x.startswith(b'STYLEOS S858 ')]
        if len(records)!=2 or records[1]!=POLICY or any(b' FAIL ' in x for x in lines):raise ValueError('858 record/policy')
        if not lines.index(SWITCH)<lines.index(records[0])<lines.index(next(x for x in lines if x.startswith(b'STYLEOS S852 FRAME ')))<lines.index(FRAME)<lines.index(POLICY):raise ValueError('858 order')
        m=re.fullmatch(b'STYLEOS S858 RUNTIME '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in KEYS),records[0])
        if not m:raise ValueError('858 schema')
        v=dict(zip(KEYS,(int(x,16) for x in m.groups())))
        active=data['active'];b=data['bridge'];d=data['descriptors']
        if (v['root'],v['old'],v['gdt'],v['idt'],v['tr'],v['cs'])!=(active['root'],b['old'],d['gdt'],d['idt'],b['selector'],b['cs']):raise ValueError('858 root/descriptor binding')
        lo,hi=next(x for x in lines if x.startswith(b'STYLEOS MEMMAP STACK ')).split()[-2:]
        if not int(lo,16)<=v['rsp']<int(hi,16):raise ValueError('858 stack')
        if v['cr0']&0x80010001!=0x80010001 or not v['cr4']&32 or v['cr4']&((1<<7)|(1<<12)|(1<<17)) or v['efer']&0xc00!=0xc00 or not v['flags']&2 or v['flags']&((1<<9)|(1<<14)|(1<<17)):raise ValueError('858 live mode')
        if (v['gdt-limit'],v['idt-limit'],v['safe-root'],v['processes'],v['windows'],v['session-next'],v['selected'],v['ports'])!=(v['tr']+15,4095,v['root'],0,0,1,0,0):raise ValueError('858 services')
        data['runtime']=v;return True,data
    except (ValueError,IndexError,KeyError,TypeError,StopIteration) as e:return False,str(e)
