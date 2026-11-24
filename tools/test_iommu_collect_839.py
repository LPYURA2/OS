#!/usr/bin/env python3
"""Host-only transactional collector test of existing 800/801/838 contracts.
Uses accepted S837 plans and synthetic table dictionaries; no live access.
"""
import ctypes as C
import json, sys, re
from iommu_guest_837_verify import run as verify837
from pathlib import Path
U=C.c_uint64; Z=C.c_size_t
class R(C.Structure): _fields_=[('start',U),('end',U)]
class P(C.Structure): _fields_=[('protected_ranges',R*512),('review',R*512),('nr_protected',Z),('nr_review',Z),('protected_pages',U),('review_pages',U),('granted',U),('valid',C.c_int)]
class T(C.Structure): _fields_=[('present',C.c_bool),('leaf_entry',U),('physical',U),('page_size',U),('pat_index',C.c_uint8),('pat_type',C.c_uint8)]
class M(C.Structure): _fields_=[('type',C.c_uint8),('matched',C.c_uint8),('mtrr_enabled',C.c_bool)]
class E(C.Structure): _fields_=[('address',U),('first',T),('last',T),('physical_cache',M)]
READ=C.CFUNCTYPE(C.c_bool,C.c_void_p,U,C.c_uint16,C.POINTER(U))
lib=C.CDLL(sys.argv[1]); walk=lib.cache801_walk; walk.argtypes=[U,U,U,C.c_uint8,READ,C.c_void_p,C.POINTER(T)]; walk.restype=C.c_bool
cache=lib.cache800_physical_type; cache.argtypes=[U,C.c_uint8,U,U,C.c_void_p,C.c_uint8,C.POINTER(M)]; cache.restype=C.c_bool
observe=lib.s838_observe; observe.argtypes=[C.POINTER(P),R,C.POINTER(E),Z]; observe.restype=C.c_int
root=Path(sys.argv[2]);report=verify837(root/'qemu')
assert report==json.loads((root/'results.json').read_text())
assert report['stage']==837 and report['status']=='PASS' and not report['allocator_integrated']
covers={'disabled':(0,0),'intel':(0xfed90000,0xfed91000),'bridge-intel':(0xfed90000,0xfed91000),'bridge-two-hop-intel':(0xfed90000,0xfed91000),'amd':(0xfed80000,0xfed84000)}
assert set(report['modes'])==set(covers)
class S(C.Structure): _fields_=[('root',U),('pat',U),('cap',U),('def_',U),('physical_bits',C.c_uint8),('count',C.c_uint8),('pairs',U*32)]
class Result(C.Structure): _fields_=[('count',Z),('pages',E*4)]
collect=lib.s839_collect;collect.argtypes=[C.POINTER(P),R,C.POINTER(S),READ,C.c_void_p,C.POINTER(Result)];collect.restype=C.c_int
results={}
for mode,(a,b) in covers.items():
    text=(root/'qemu'/mode/'serial.log').read_text(encoding='ascii')
    covers_raw=re.findall(r'^STYLEOS S835 COVER start=0x([0-9a-f]{16}) end=0x([0-9a-f]{16})$',text,re.M)
    assert len(covers_raw)==1 and tuple(int(v,16) for v in covers_raw[0])==(a,b)
    p=P();p.valid=1;row=report['modes'][mode]; assert row['granted']==0 and not row['allocator_integrated']
    for field,key,num,total in [('protected_ranges','protected','nr_protected','protected_pages'),('review','review','nr_review','review_pages')]:
        for i,(lo,hi) in enumerate(row[key]):getattr(p,field)[i]=R(lo,hi)
        setattr(p,num,len(row[key]));setattr(p,total,sum((hi-lo)//4096 for lo,hi in row[key]))
    checks=0; reads=0;n=(b-a)//4096
    snap=S(0x1000,6,0,0x800,48,0)
    table={}
    @READ
    def read(_ctx,pa,slot,out):
        global reads
        reads+=1
        if (pa,slot) not in table:return False
        out[0]=table[pa,slot];return True
    null=READ()
    def check(want,plan=p,cover=R(a,b),snapshot=snap,reader=read,zero_reads=False):
        global checks
        old=bytes(plan); src=bytes(snapshot) if snapshot is not None else None
        out=Result();C.memset(C.byref(out),0xa5,C.sizeof(out));before=bytes(out);start=reads
        ptr=C.byref(snapshot) if snapshot is not None else None
        assert collect(C.byref(plan),cover,ptr,reader,None,C.byref(out))==want,(mode,checks,want)
        assert bytes(plan)==old and (snapshot is None or bytes(snapshot)==src)
        if not want:assert bytes(out)==before
        else:
            assert out.count==n and observe(C.byref(plan),cover,out.pages,out.count)==1
            for i in range(n):assert out.pages[i].address==a+4096*i
        if zero_reads:assert reads==start
        checks+=1
    # Invalid plans and covers fail before any callback.
    bad=P.from_buffer_copy(bytes(p));bad.granted=1
    check(0,plan=bad,zero_reads=True)
    check(0,cover=R(1,2),zero_reads=True)
    old=bytes(p);start=reads
    assert not collect(C.byref(p),R(a,b),C.byref(snap),read,None,C.cast(C.byref(p),C.POINTER(Result))) and bytes(p)==old and reads==start;checks+=1
    # The output must not overlap a supplied snapshot, even with zero pages.
    old=bytes(snap);start=reads
    assert not collect(C.byref(p),R(a,b),C.byref(snap),read,None,C.cast(C.byref(snap),C.POINTER(Result))) and bytes(snap)==old and reads==start;checks+=1
    if not n:
        check(1,snapshot=None,reader=null,zero_reads=True)
        check(1,zero_reads=True)
    else:
        check(0,snapshot=None,zero_reads=True);check(0,reader=null,zero_reads=True)
        for k,v in [('physical_bits',35),('physical_bits',53),('count',17),('cap',1)]:
            bad=S.from_buffer_copy(bytes(snap));setattr(bad,k,v);check(0,snapshot=bad,zero_reads=True)
        bad=P.from_buffer_copy(bytes(p));bad.nr_protected=0;bad.protected_pages=0;check(0,plan=bad,zero_reads=True)
        for size in [4096,0x200000,0x40000000]:
            for variant in ['valid','absent','unreadable','wrong_translation','cache_disabled','cache_wb','pat_unsupported']:
                table.clear()
                for x in range(a,b,4096):
                    table[0x1000,(x>>39)&511]=0x2001
                    if size==0x40000000:table[0x2000,(x>>30)&511]=(x&~(size-1))|129
                    else:
                        table[0x2000,(x>>30)&511]=0x3001
                        if size==0x200000:table[0x3000,(x>>21)&511]=(x&~(size-1))|129
                        else:
                            table[0x3000,(x>>21)&511]=0x4001
                            table[0x4000,(x>>12)&511]=x|1
                leafaddr,shift={4096:(0x4000,12),0x200000:(0x3000,21),0x40000000:(0x2000,30)}[size]
                key=(leafaddr,(a>>shift)&511)
                if variant=='absent':table[key]=0
                elif variant=='unreadable':del table[key]
                elif variant=='wrong_translation':table[key]+=size
                s=S.from_buffer_copy(bytes(snap));s.pat=1 if variant=='pat_unsupported' else 6
                s.def_=0 if variant=='cache_disabled' else 0x806 if variant=='cache_wb' else 0x800
                check(int(variant=='valid'),snapshot=s)
        # A failure late in the bounded collection cannot expose a partial report.
        if n>1:
            table.clear()
            for x in range(a,b,4096):
                table[0x1000,(x>>39)&511]=0x2001;table[0x2000,(x>>30)&511]=0x3001;table[0x3000,(x>>21)&511]=0x4001;table[0x4000,(x>>12)&511]=x|1
            del table[0x4000,((b-4096)>>12)&511];check(0)
    results[mode]={'checks':checks,'live_mapping_observed':False}
report={'stage':839,'mode':'host-only collector contract','checks':sum(x['checks'] for x in results.values()),'modes':results}
Path(sys.argv[3]).write_text(json.dumps(report,indent=2)+'\n')
print(f"{report['checks']}/{report['checks']} PASS; five verified S837 plans; synthetic collector inputs")
