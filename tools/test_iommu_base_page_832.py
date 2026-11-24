import struct
from iommu_base_page_832 import census
P=4096;B=0xfed90000;U={'base':B,'segment':0,'requester':None,'type':0}
def desc(t,a,b,attr=0):return struct.pack('<I4xQQQQ',t,a,0,(b-a)//P,attr)+bytes(8)
def run(raw,units=None,ecam=None,windows=(),ioapics=(),candidates=(),reserved=(),stride=48,version=1):
    return census(raw,stride,version,[U] if units is None else units,[0xe0000000,0xf0000000] if ecam is None else ecam,windows,ioapics,candidates,reserved)
n=0
for raw,want in [(desc(0,B,B+P),'EXPLICIT_NON_RAM_COVERAGE'),(desc(11,B,B+P),'EXPLICIT_NON_RAM_COVERAGE'),(desc(7,B,B+P),'CONFLICT'),(desc(5,B,B+P),'CONFLICT'),(desc(0,B,B+P,1<<63),'CONFLICT'),(desc(0x80000000,B,B+P),'CONFLICT'),(desc(7,P,2*P),'GAP'),(desc(0,B-P,B),'GAP'),(desc(11,B+P,B+2*P),'GAP')]:
    r=run(raw)[0];assert r['status']==want and not any(r[k] for k in ('full_aperture_verified','ownership','mmio_authorized','dma_authorized'));n+=1
for k in ('ecam','windows','ioapics','candidates'):
    r=run(desc(0,B,B+P),**{k:[B,B+P] if k=='ecam' else [[B,B+P]]})[0];assert r['status']=='CONFLICT';n+=1
r=run(desc(7,P,2*P),reserved=[[B,B+P]])[0];assert r['status']=='GAP' and r['reserved_in_detached_map'];n+=1
assert run(desc(7,P,2*P),units=[])==[];n+=1
for raw,kw in [(b'',{}),(desc(0,B,B+P)[:-1],{}),(desc(0,B,B+P)+desc(11,B,B+P),{}),(desc(0,B,B+P),{'version':2}),(desc(0,B,B+P),{'stride':39}),(desc(0,B,B+P),{'units':[U,U]}),(desc(0,B,B+P),{'units':[{**U,'base':B+1}]}),(desc(0,B,B+P),{'units':[{**U,'base':2**64}]}),(desc(0,B,B+P),{'units':[{**U,'segment':1}]}),(b'',{'units':[]})]:
    try:run(raw,**kw)
    except ValueError:n+=1
    else:raise AssertionError('bad fixture accepted')
print(f'{n}/{n} synthetic first-page cases PASS; no grants')
