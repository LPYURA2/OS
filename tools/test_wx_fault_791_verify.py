#!/usr/bin/env python3
"""Negative tests: fabricated UART and broken policy cannot pass the 791 verifier."""
from wx_fault_791_verify import verify,parse,KEYS,TRAILERS,END

def record(name,fields):
    return b'STYLEOS S791 '+name.encode()+b' '+b' '.join(k.encode()+b'=0x'+f'{fields[k]:016x}'.encode() for k in KEYS[name])+TRAILERS[name]

def fixture(mode):
    old=0x7801000;new=0x5fdb000;low=0x5fc5000
    alias=0x400000003000;rx=0x5a015000;nx=0x5a110000
    a=dict(mode=mode,old=old,new=new,alias=alias,frame=0x5fe5000,cs=0x38,
        selector=0x48,idt=0x5fe4000,ist2=alias+0x3000,guard=alias-0x1000,
        frame2=0x5fe8000,ist3=alias+0x6000,frame3=0x5fe9000,
        root0=(low+0x1000)|0x23,root128=0x5fe2003,oldroot0=0x7781023,
        **{'guard-lo':alias-0x1000,'guard-hi':alias+0x1000,'low-table':low,
           'low-used':4,'code-rx':16,'data-rw-nx':195,'ro-nx':5,
           'stack-rw-nx':17,'rx-target':rx,'nx-target':nx})
    b={'cr3':new,'rsp':alias+0xfc8,'saved-rip':rx+0x123,'expected-rip':rx+0x123,'tr':0x48}
    target=rx if mode==1 else nx
    p={'mode':mode,'cr2':target,'err':3 if mode==1 else 17,
        'rsp':alias+0x3000+0xfc0,'rip':target+0x456 if mode==1 else target,
        'expected-rip':target+0x456 if mode==1 else target,
        'target':target,'old-rsp':alias+0xf60,'cr3':new,'tr':0x48,
        'cr0':0x80010033,'efer':0xd00,'leaf-flags':2 if mode==1 else 1,
        'low-table':low,'root0':a['root0'],'root128':a['root128']}
    return a,b,p

def encode(triplet):
    return b'\n'.join(record(name,fields) for name,fields in zip(KEYS,triplet))+b'\n'+END+b'\n'

def main():
    n=0
    for mode in (1,2):
        original=list(fixture(mode))
        raw=encode(original)
        # Synthetic positive is merely a parser UNIT TEST, NEVER evidence of hardware.
        assert verify(raw,33,mode)[0],verify(raw,33,mode)
        mutants=[(None,34,mode),(None,33,3),(b'STYLEOS S791 FAIL corruption\n',33,mode),
                 (raw+END+b'\n',33,mode),(raw.replace(b'pf-ist2=1',b'pf-ist2=0'),33,mode)]
        for change_rc,rc,check_mode in mutants:
            bad=raw if change_rc is None else (raw+change_rc if change_rc.startswith(b'STYLEOS S791 FAIL') else change_rc)
            assert not verify(bad,rc,check_mode)[0]
            n+=1
        for scope,key in ((0,'root0'),(0,'old'),(0,'rx-target' if mode==1 else 'nx-target'),(1,'rsp'),
                          (2,'err'),(2,'cr2'),(2,'rip'),(2,'cr3'),(2,'tr'),
                          (2,'efer'),(2,'cr0'),(2,'leaf-flags'),(2,'old-rsp'),(2,'root128')):
            f=[dict(x) for x in original];f[scope][key]^=(1<<13) if key=='old-rsp' else ((1<<16) if key=='cr0' else ((1<<11) if key=='efer' else 1))
            assert not verify(encode(f),33,mode)[0],(mode,key)
            n+=1
    print(f'STYLEOS 791 UART verifier: {n} negative mutations rejected; synthetic fixtures are NOT QEMU evidence')
if __name__=='__main__':main()
