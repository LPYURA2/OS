"""Use independently accepted original S837 plans, synthetic translation observations.
No target register access or live translation verification here.
"""
import ctypes as C, json, re, sys
from iommu_guest_837_verify import run as verify837
from pathlib import Path
U=C.c_uint64; Z=C.c_size_t
class R(C.Structure):_fields_=[('start',U),('end',U)]
class P(C.Structure):_fields_=[('protected_ranges',R*512),('review',R*512),('nr_protected',Z),('nr_review',Z),('protected_pages',U),('review_pages',U),('granted',U),('valid',C.c_int)]
class T(C.Structure):_fields_=[('present',C.c_bool),('leaf_entry',U),('physical',U),('page_size',U),('pat_index',C.c_uint8),('pat_type',C.c_uint8)]
class M(C.Structure):_fields_=[('type',C.c_uint8),('matched',C.c_uint8),('mtrr_enabled',C.c_bool)]
class E(C.Structure):_fields_=[('address',U),('first',T),('last',T),('physical_cache',M)]
fn=C.CDLL(sys.argv[1]).s838_observe;fn.argtypes=[C.POINTER(P),R,C.POINTER(E),Z];fn.restype=C.c_int
root=Path(sys.argv[2]);report=verify837(root/'qemu');assert report==json.loads((root/'results.json').read_text())
assert report['status']=='PASS' and not report['allocator_integrated']
assert set(report['modes'])=={'disabled','intel','bridge-intel','bridge-two-hop-intel','amd'}
counts=0;summary={}
def setranges(p,k,rows):
    for i,(a,b) in enumerate(rows):getattr(p,k)[i]=R(a,b)
def npage(rows):return sum((b-a)//4096 for a,b in rows)
def clone(v):return type(v).from_buffer_copy(bytes(v))
for mode,row in report['modes'].items():
    raw=(root/'qemu'/mode/'serial.log').read_text(encoding='ascii')
    match=re.findall(r'STYLEOS S835 COVER start=0x([0-9a-f]{16}) end=0x([0-9a-f]{16})',raw);assert len(match)==1
    a,b=map(lambda s:int(s,16),match[0]);c=R(a,b);n=(b-a)//4096
    p=P();p.valid=1;pr=row['protected'];rv=row['review'];setranges(p,'protected_ranges',pr);setranges(p,'review',rv);p.nr_protected=len(pr);p.nr_review=len(rv);p.protected_pages=npage(pr);p.review_pages=npage(rv);assert row['granted']==0 and not row['allocator_integrated']
    es=(E*max(n,1))()
    for i in range(n):
        x=a+i*4096;es[i]=E(x,T(True,x|1,x,4096,0,6),T(True,x|1,x+4095,4096,0,6),M(0,1,True))
    start=counts
    def call(plan=p,cover=c,pages=es,count=n,good=True):
        global counts
        old=bytes(plan);prev=bytes(pages) if pages is not None else None
        assert fn(C.byref(plan),cover,pages,count)==int(good),(mode,counts,good)
        assert bytes(plan)==old and (pages is None or bytes(pages)==prev)
        counts+=1
    call()
    for k,v in [('valid',0),('granted',1),('nr_protected',513),('nr_review',513),('protected_pages',p.protected_pages+1),('review_pages',p.review_pages+1)]:
        q=clone(p);setattr(q,k,v);call(plan=q,good=False)
    q=clone(p);q.review[0].start+=1;call(plan=q,good=False)
    q=clone(p);q.protected_ranges[0].end+=1;call(plan=q,good=False)
    call(cover=R(1,2),good=False);call(count=n+1,good=False)
    if n:
        call(pages=None,good=False);call(count=n-1,good=False)
        # Remove exact cover from the original protected set while keeping totals valid.
        rem=[]
        for x,y in pr:
            if x<a<y:rem.append([x,a])
            if x<b<y:rem.append([b,y])
            if y<=a or b<=x:rem.append([x,y])
        q=clone(p);setranges(q,'protected_ranges',rem);q.nr_protected=len(rem);q.protected_pages=npage(rem);call(plan=q,good=False)
        for i in range(n):
            for target,k,v in [('first','present',False),('last','present',False),('first','physical',a-4096),('last','physical',a),('first','leaf_entry',0),('last','leaf_entry',7),('first','page_size',8192),('first','pat_index',7),('last','pat_type',1),('physical_cache','type',6),('physical_cache','mtrr_enabled',False)]:
                e=clone(es);setattr(getattr(e[i],target),k,v);call(pages=e,good=False)
            e=clone(es);e[i].address+=4096;call(pages=e,good=False)
            e=clone(es);e[i].first.leaf_entry^=4096;e[i].last.leaf_entry=e[i].first.leaf_entry;call(pages=e,good=False)
            e=clone(es);e[i].physical_cache.matched=17;call(pages=e,good=False)
            for size in (0x200000,0x40000000):
                e=clone(es);x=a+i*4096
                for t in (e[i].first,e[i].last):t.page_size=size;t.leaf_entry=(x&~(size-1))|129
                call(pages=e)
                for t in (e[i].first,e[i].last):t.leaf_entry&=~128
                call(pages=e,good=False)
                for t in (e[i].first,e[i].last):t.leaf_entry|=128|8192
                call(pages=e,good=False)
    else:call(pages=None);call(cover=R(0,4096),good=False)
    summary[mode]={'synthetic_pages':n,'tests':counts-start,'live_mapping_observed':False}
print(f'{counts}/{counts} PASS; 5 original S837 plans; synthetic translation observations only')
Path(sys.argv[3]).write_text(json.dumps(summary,indent=2)+'\n')
