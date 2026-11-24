"""Native input derivation and exact UART contract; synthetic UART, original S834 maps."""
import ctypes as c,sys,json,hashlib,copy
from pathlib import Path
from irq_sources_830_verify import verify,BACKEND
from test_acpi_windows_825 import reconcile
from iommu_quarantine_835_verify import records,verify_native
from post_ebs_aperture_834 import READY,ACK
class R(c.Structure):_fields_=[('start',c.c_uint64),('end',c.c_uint64)]
class I(c.Structure):_fields_=[('backend',c.c_uint),('aperture',R),('others',R*256),('nr_others',c.c_size_t)]
fn=c.CDLL(sys.argv[1]).s835_input_from_tables
fn.argtypes=[c.c_uint]+[c.c_void_p,c.c_size_t]*4+[c.POINTER(I)];fn.restype=c.c_int
n=0;uart_n=0
for mode,backend in BACKEND.items():
 p=Path(sys.argv[2])/mode
 if mode=='amd' and not p.exists():continue
 raw=(p/'serial.log').read_bytes();gated=(p/'serial-gated.log').read_bytes();t=json.loads((p/'qmp-834.json').read_text())
 ok,d=verify(raw,33,mode);assert ok;w=reconcile(d)
 blobs=[(p/f).read_bytes() for f in ('iommu-table.bin','madt.bin','crs.bin','uefi-root-0.bin')]
 def run(bs,be=backend,good=True):
  global n
  out=I();c.memset(c.byref(out),0xa5,c.sizeof(out));before=bytes(out)
  args=[v for b in bs for v in (b,len(b))];assert fn(be,*args,c.byref(out))==int(good),mode
  if not good:assert bytes(out)==before
  n+=1;return out
 v=run(blobs);want=[v[1:] for v in w['producer_windows'] if v[0]==0]+[[v['start'],v['end']] for v in w['allocated_windows'] if v['kind']==0]+[[v['address'],v['address']+4096] for v in d['s830_ioapics']]
 assert [[v.others[i].start,v.others[i].end] for i in range(v.nr_others)]==want
 for j,b in enumerate(blobs):
  if not b:continue
  bs=list(blobs);bs[j]=b[:-1];run(bs,good=False)
 run(blobs,3,False)
 lines,report=records(gated,33,mode,t);assert [v.aperture.start,v.aperture.end]==report['aperture']
 anchor=b'STYLEOS S827 POLICY detached-map=yes ram-grants=no ecam=not-read dma-grants=no production-cr3=deny';assert raw.count(anchor)==1
 # QEMU line endings from the original UART are preserved.
 pos=raw.index(b'\n',raw.index(anchor))+1;block=('\n'.join(lines)+'\n').encode();new=raw[:pos]+block+raw[pos:]
 tail=gated[len(raw):];ready,ack=tail.splitlines(keepends=True);q=copy.deepcopy(t);q['ready_sha256']=hashlib.sha256(new+ready).hexdigest();sample=new+tail
 assert verify_native(sample,33,mode,q)==report;uart_n+=1
 for changed in (b'',block+block,block.replace(b'ram-grants=no',b'ram-grants=yes'),block.replace(b'APERTURE',b'FAKE'),block.replace(b'gap-pages=',b'fake-pages='),block.replace(b'RESERVED',b'CANDIDATE',1)):
  fake=sample.replace(block,changed,1);q2=copy.deepcopy(q);q2['ready_sha256']=hashlib.sha256(fake[:-len(ack)]).hexdigest()
  try:verify_native(fake,33,mode,q2)
  except ValueError:uart_n+=1
  else:raise AssertionError('forged S835 accepted')
print(f'{n}/{n} native fixture derivation/refusals PASS; {uart_n}/{uart_n} synthetic S835 UART cases PASS')
