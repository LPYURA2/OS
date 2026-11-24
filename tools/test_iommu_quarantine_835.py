"""Host C replay through ctypes; original S833 initial QMP + UEFI only."""
import ctypes as c, json, sys, struct, hashlib
from pathlib import Path
from iommu_aperture_833 import correlate
from post_ebs_aperture_834 import replay as replay834, ungate
from irq_sources_830_verify import verify, BACKEND
from test_acpi_windows_825 import reconcile
U=c.c_uint64; Z=c.c_size_t
class R(c.Structure):_fields_=[('start',U),('end',U)]
class M(c.Structure):_fields_=[('ram',R*256),('reserved',R*256),('candidates',R*256),('nr_ram',Z),('nr_reserved',Z),('nr_candidates',Z)]
class E(c.Structure):_fields_=[('ecam',R),('explicit',U),('gap',U),('diagnostic',M)]
class I(c.Structure):_fields_=[('backend',c.c_uint),('aperture',R),('others',R*256),('nr_others',Z)]
class O(c.Structure):_fields_=[('aperture',R),('cover',R),('explicit',U),('gap',U),('diagnostic',M)]
class W(c.Structure):_fields_=[('ew',E),('e',E),('result',O)]
lib=c.CDLL(sys.argv[1]);fn=lib.s835_quarantine
fn.argtypes=[c.POINTER(I),c.c_void_p,Z,c.c_void_p,Z,c.c_void_p,Z,Z,c.c_uint32,c.POINTER(O),c.POINTER(W)];fn.restype=c.c_int
root=Path(sys.argv[2]);counts=0;results={}
def pair(v):return [v.start,v.end]
def ranges(m,k):return [pair(v) for v in getattr(m,k)[:getattr(m,'nr_'+k)]]
def desc(t,a,b,attr=0,stride=48):return struct.pack('<I4xQQQQ',t,a,0,(b-a)//4096,attr)+bytes(stride-40)
for mode,backend in BACKEND.items():
 p=root/mode
 if mode=='amd' and not p.exists():continue
 raw=(p/'serial.log').read_bytes()
 if (p/'qmp-834.json').exists():
  trace=json.loads((p/'qmp-834.json').read_text());gated=(p/'serial-gated.log').read_bytes();assert ungate(gated)==raw
  final=replay834(gated,33,mode,trace);want=correlate(raw,33,mode,trace['initial']);assert final['detached_denied']==want['detached_denied'] and final['unchanged_candidates']==want['unchanged_candidates']
 else:
  trace=json.loads((p/'qmp-833.json').read_text());want=correlate(raw,33,mode,trace)
 ok,d=verify(raw,33,mode);assert ok
 e=d['s826_map_audit'];w=reconcile(d);others=[v[1:] for v in w['producer_windows'] if v[0]==0]+[[v['start'],v['end']] for v in w['allocated_windows'] if v['kind']==0]+[[v['address'],v['address']+4096] for v in d['s830_ioapics']]
 i=I();i.backend=backend;i.aperture=R(*(want['initial_qemu_model_apertures'][0]['interval'] if backend else [0,0]));i.nr_others=len(others)
 for j,v in enumerate(others):i.others[j]=R(*v)
 mcfg=(p/'mcfg.bin').read_bytes();drac=(p/'drac-crs.bin').read_bytes();mem=(p/'final-map.bin').read_bytes();stride=e['descriptor_size']
 def run(in_=i,mem_=mem,m=mcfg,dr=drac,s=stride,version=1,good=True,alias=False):
  global counts
  o=O();c.memset(c.byref(o),0xa5,c.sizeof(o));before=bytes(o);work=W();ptr=c.cast(c.byref(o),c.POINTER(W)) if alias else c.byref(work)
  rc=fn(c.byref(in_),m,len(m),dr,len(dr),mem_,len(mem_),s,version,c.byref(o),ptr)
  assert rc==int(good),(mode,rc,good)
  if not good:assert bytes(o)==before
  counts+=1;return o
 o=run();assert ranges(o.diagnostic,'reserved')==want['detached_denied'];assert ranges(o.diagnostic,'candidates')==want['unchanged_candidates']
 if backend:assert pair(o.cover)==want['initial_qemu_model_apertures'][0]['denied_page_interval'] and not o.explicit
 else:assert pair(o.cover)==[0,0] and not o.gap
 results[mode]={'aperture':pair(o.aperture),'cover':pair(o.cover),'gap_pages':o.gap,'explicit_pages':o.explicit,'reserved':ranges(o.diagnostic,'reserved'),'candidates':ranges(o.diagnostic,'candidates')}
 for kwargs in ({'mem_':mem[:-1]},{'mem_':b''},{'version':2},{'s':39},{'m':mcfg[:-1]},{'dr':drac[:-1]},{'alias':True}):run(**kwargs,good=False)
 for change in (lambda q:setattr(q,'backend',3),lambda q:setattr(q,'nr_others',257),lambda q:setattr(q,'aperture',R(1,2)),lambda q:(setattr(q,'nr_others',1),q.others.__setitem__(0,R(5,5)))):
  q=I.from_buffer_copy(bytes(i));change(q);run(in_=q,good=False)
 if backend:
  a,b=pair(o.cover)
  for t,attr in [(7,8),(5,0),(11,1<<63),(12,0),(0x80000000,0)]:run(mem_=mem+desc(t,a,b,attr,stride),good=False)
  for t in (0,11):
   v=run(mem_=mem+desc(t,a,b,0,stride));assert v.explicit==(b-a)//4096 and not v.gap
  q=I.from_buffer_copy(bytes(i));q.nr_others=1;q.others[0]=R(b-1,b);run(in_=q,good=False)
  q.others[0]=R(b,b+1);run(in_=q)
print(f'{counts}/{counts} C replay and refusal cases PASS; {len(results)} original modes; '+('S834 paired QMP/UEFI replay' if (root/'intel'/'qmp-834.json').exists() else 'S833 initial QMP/UEFI only'))
Path(sys.argv[3]).write_text(json.dumps(results,indent=2)+'\n')
