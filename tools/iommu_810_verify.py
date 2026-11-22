"""Strict raw UART verifier for terminal x86 ACPI IOMMU inventory. No DMA claim."""
import re
RECORD=re.compile(r'^STYLEOS S810 PLATFORM backend=0x([0-9a-f]{16}) root-entries=0x([0-9a-f]{16})$')
UNIT=re.compile(r'^STYLEOS S810 UNIT type=0x([0-9a-f]{16}) index=0x([0-9a-f]{16}) segment=0x([0-9a-f]{16}) base=0x([0-9a-f]{16}) scope-count=0x([0-9a-f]{16})$')
INTEL=re.compile(r'^STYLEOS S810 INTEL drhd=0x([0-9a-f]{16}) rmrr=0x([0-9a-f]{16}) unknown=0x([0-9a-f]{16})$')
AMD=re.compile(r'^STYLEOS S810 AMD ivhd=0x([0-9a-f]{16}) ivmd=0x([0-9a-f]{16}) unknown=0x([0-9a-f]{16}) opaque-scopes=0x([0-9a-f]{16})$')
POLICY='STYLEOS S810 POLICY inventory-only=yes pci-requesters-verified=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny'
def verify(raw:bytes,rc:int|None,expected:str):
 if rc!=33 or not raw or b'\x00' in raw or len(raw)>120_000:return False,{}
 try:s=raw.decode('ascii').replace('\r','')
 except UnicodeDecodeError:return False,{}
 lines=s.splitlines()
 if any('STYLEOS S810 FAIL' in x for x in lines):return False,{}
 records=[x for x in lines if x.startswith('STYLEOS S810 ')]
 m=[RECORD.fullmatch(x) for x in records if x.startswith('STYLEOS S810 PLATFORM')]
 if len(m)!=1 or m[0] is None:return False,{}
 backend,root=map(lambda x:int(x,16),m[0].groups())
 want={'disabled':0,'intel':1,'amd':2}.get(expected)
 if backend!=want or root<1 or root>64:return False,{}
 declarations=[x for x in records if x.startswith('STYLEOS S810 INTEL') or x.startswith('STYLEOS S810 AMD')]
 units=[x for x in records if x.startswith('STYLEOS S810 UNIT')]
 if expected=='disabled':
  if declarations or units:return False,{}
  count=0;detail={}
 else:
  if len(declarations)!=1:return False,{}
  pattern=INTEL if expected=='intel' else AMD
  r=pattern.fullmatch(declarations[0])
  if not r:return False,{}
  vals=[int(x,16) for x in r.groups()]
  count=vals[0]
  if not 1<=count<=16 or len(units)!=count:return False,{}
  detail=dict(zip(('drhd','rmrr','unknown') if expected=='intel' else ('ivhd','ivmd','unknown','opaque_scopes'), vals))
  for i,line in enumerate(units):
   u=UNIT.fullmatch(line)
   if not u:return False,{}
   typ,ix,seg,base,scopes=(int(x,16) for x in u.groups())
   if ix!=i or seg>65535 or not base or base&4095 or scopes>64:return False,{}
   if expected=='intel' and typ!=0:return False,{}
   if expected=='amd' and typ not in (0x10,0x11,0x40):return False,{}
 if records.count(POLICY)!=1 or records.count('STYLEOS S810 PASS')!=1:return False,{}
 if records[-1]!='STYLEOS S810 PASS':return False,{}
 if len(records)!=count+3+(1 if expected!='disabled' else 0):return False,{}
 return True,{'backend':expected,'root_entries':root,'units':count,'details':detail,
  'iommu_translation_enabled':False,'dma_isolation_verified':False,
  'pci_requesters_verified':False,'dma_access_authorized':False,'production_cr3_authorized':False}
