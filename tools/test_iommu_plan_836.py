"""Original S835 raw map/report, synthetic prior ADR-0008 plan. No guest execution."""
import ctypes as C, json, struct, sys
from iommu_quarantine_835_verify import run as verify835
from pathlib import Path
U=C.c_uint64; Z=C.c_size_t; PAGE=4096
class R(C.Structure):_fields_=[('start',U),('end',U)]
class M(C.Structure):_fields_=[('ram',R*256),('reserved',R*256),('candidates',R*256),('nr_ram',Z),('nr_reserved',Z),('nr_candidates',Z)]
class Q(C.Structure):_fields_=[('aperture',R),('cover',R),('explicit_pages',U),('gap_pages',U),('diagnostic',M)]
class P(C.Structure):_fields_=[('protected_ranges',R*512),('review',R*512),('nr_protected',Z),('nr_review',Z),('protected_pages',U),('review_pages',U),('granted',U),('valid',C.c_int)]
class W(C.Structure):_fields_=[('plan',P),('checked',M)]
f=C.CDLL(sys.argv[1]).s836_extend;f.argtypes=[C.POINTER(P),C.POINTER(Q),C.POINTER(P),C.POINTER(W)];f.restype=C.c_int
root=Path(sys.argv[2]);parent=verify835(root/'qemu')
assert parent==json.loads((root/'results.json').read_text()),'S835 report mismatch'
meta=json.loads((root/'qemu/results.json').read_text());reports=parent['modes'];count=0;summary={}
def assign(o,k,spans):
 for i,(a,b) in enumerate(spans):getattr(o,k)[i]=R(a,b)
def rows(o,k,n):return [[r.start,r.end] for r in getattr(o,k)[:n]]
def pages(spans):return sum((b-a)//PAGE for a,b in spans)
def merge(spans):
 out=[]
 for a,b in sorted(spans):
  if out and a<=out[-1][1]:out[-1][1]=max(out[-1][1],b)
  else:out.append([a,b])
 return out
def call(p,q,good=True,alias=False):
 global count
 oldp=bytes(p);oldq=bytes(q);o=P();C.memset(C.byref(o),0xa5,C.sizeof(o));old=bytes(o);w=W()
 ptr=C.cast(C.byref(o),C.POINTER(W)) if alias else C.byref(w)
 assert f(C.byref(p),C.byref(q),C.byref(o),ptr)==int(good)
 assert bytes(p)==oldp and bytes(q)==oldq
 if not good:assert bytes(o)==old
 count+=1;return o
for mode,r in reports.items():
 if r.get('status')=='SKIPPED':continue
 obs=meta['modes'][mode]['observations'];data=(root/'qemu'/mode/'final-map.bin').read_bytes();stride=obs['s826_map_audit']['descriptor_size']
 ram=[]
 for off in range(0,len(data),stride):
  t,start,_,n,attr=struct.unpack_from('<I4xQQQQ',data,off)
  if t==7 and attr&8 and not attr&~0x401f:ram.append([start,start+n*PAGE])
 q=Q();q.aperture=R(*r['aperture']);q.cover=R(*r['cover']);q.explicit_pages=r['explicit_pages'];q.gap_pages=r['gap_pages'];m=q.diagnostic
 for k,spans in [('ram',sorted(ram)),('reserved',r['reserved']),('candidates',r['candidates'])]:assign(m,k,spans);setattr(m,'nr_'+k,len(spans))
 # Simulated prior ownership output: actual baseline reservations/review only.
 prior=obs['s827_exclusion'];p=P();p.valid=1;p.nr_protected=len(prior['reserved']);assign(p,'protected_ranges',prior['reserved']);p.protected_pages=pages(prior['reserved']);p.nr_review=len(prior['candidates']);assign(p,'review',prior['candidates']);p.review_pages=pages(prior['candidates'])
 out=call(p,q);want=merge(prior['reserved']+r['reserved']);assert rows(out,'protected_ranges',out.nr_protected)==want and rows(out,'review',out.nr_review)==prior['candidates'] and not out.granted and out.valid==1 and out.protected_pages==pages(want)
 summary[mode]={'protected_pages_before':p.protected_pages,'protected_pages_after':out.protected_pages,'review_pages':p.review_pages,'granted':out.granted,'review_unchanged':True,'prior_plan_synthetic':True}
 for attr,v in [('valid',0),('granted',1),('protected_pages',p.protected_pages+1),('review_pages',p.review_pages+1),('nr_review',513),('nr_protected',513)]:
  x=P.from_buffer_copy(bytes(p));setattr(x,attr,v);call(x,q,False)
 x=P.from_buffer_copy(bytes(p));x.review[0].start+=1;call(x,q,False)
 x=P.from_buffer_copy(bytes(p));x.protected_ranges[0]=x.review[0];x.protected_pages=pages(rows(x,'protected_ranges',x.nr_protected));call(x,q,False)
 for attr,v in [('nr_ram',257),('nr_reserved',257),('nr_candidates',257),('nr_ram',0),('nr_candidates',0)]:
  y=Q.from_buffer_copy(bytes(q));setattr(y.diagnostic,attr,v);call(p,y,False)
 y=Q.from_buffer_copy(bytes(q));y.diagnostic.candidates[0].end-=PAGE;call(p,y,False)
 y=Q.from_buffer_copy(bytes(q));y.gap_pages+=1;call(p,y,False)
 y=Q.from_buffer_copy(bytes(q));y.cover.end+=PAGE;call(p,y,False)
 call(p,q,False,alias=True)
 # All caller-owned input/output/workspace aliases must fail transactionally.
 for which in ('out-plan','out-map','work-plan','work-map'):
  op=bytes(p);oq=bytes(q);o=P();C.memset(C.byref(o),0xa5,C.sizeof(o));before=bytes(o);w=W()
  outptr=C.cast(C.byref(p if which=='out-plan' else q),C.POINTER(P)) if which.startswith('out-') else C.byref(o)
  workptr=C.cast(C.byref(p if which=='work-plan' else q),C.POINTER(W)) if which.startswith('work-') else C.byref(w)
  assert not f(C.byref(p),C.byref(q),outptr,workptr) and bytes(p)==op and bytes(q)==oq and bytes(o)==before
  count+=1
 if q.cover.end:
  x=P.from_buffer_copy(bytes(p));x.nr_review=1;x.review[0]=R(q.cover.start,q.cover.end);x.review_pages=pages([r['cover']]);call(x,q,False)
 # Additional already protected range is retained, provided it does not intersect REVIEW.
 a=1<<40;x=P.from_buffer_copy(bytes(p));spans=merge(prior['reserved']+[[a,a+PAGE]]);x.nr_protected=len(spans);assign(x,'protected_ranges',spans);x.protected_pages=pages(spans);o=call(x,q);assert rows(o,'protected_ranges',o.nr_protected)==merge(spans+r['reserved'])
print(f'{count}/{count} PASS across {len(summary)} original S835 maps; synthetic prior ownership plans only')
Path(sys.argv[3]).write_text(json.dumps(summary,indent=2)+'\n')
