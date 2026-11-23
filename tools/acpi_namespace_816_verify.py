"""Independent S816 UART verifier. Old S815 grammar is replayed verbatim
for PCI/MCFG/DSDT; additional SSDT and bounded AML claims checked separately.
"""
import hashlib
import re
from acpi_hostbridge_815_verify import verify as old_verify, POLICY as POLICY815
H=r'0x([0-9a-f]{16})'
NAMESPACE=re.compile(r'^STYLEOS S816 NAMESPACE tables='+H+r' ssdts='+H+
 r' objects='+H+r' complete='+H+r' literal-ready='+H+r' seg-known='+H+
 r' bbn-known='+H+r' crs-known='+H+r' regions-denied='+H+
 r' unsupported='+H+r' duplicates='+H+'$')
HEADER=re.compile(r'^STYLEOS S816 SSDT index='+H+r' bytes='+H+'$')
CHUNK=re.compile(r'^STYLEOS S816 SSDTHEX index='+H+r' offset='+H+r' data=([0-9a-f]{2,64})$')
POLICY=('STYLEOS S816 POLICY acpi-dsdt=checksummed acpi-aml=literal-subset-only '
 'acpi-namespace-complete=no ssdt=checksummed-no-side-effects '
 'acpi-crs-evaluated=no acpi-seg-evaluated=no acpi-bbn-evaluated=no '
 'ecam-reservation-verified=no ecam-dereference=no iommu-ownership-complete=no '
 'dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny')
def verify(raw:bytes,rc:int|None,mode:str):
 if rc!=33 or not raw or len(raw)>1050000 or b'\0' in raw or not raw.endswith(b'\n'):
  return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 if any('STYLEOS S816 FAIL' in line for line in lines):return False,{}
 records=[line for line in lines if line.startswith('STYLEOS S816 ')]
 if len(records)<12 or records[-2:]!=[POLICY,'STYLEOS S816 PASS']:return False,{}
 key=[line for line in records if line.startswith('STYLEOS S816 NAMESPACE')]
 if len(key)!=1 or not NAMESPACE.fullmatch(key[0]):return False,{}
 vals=[int(v,16) for v in NAMESPACE.fullmatch(key[0]).groups()]
 tables,ssdts,objects,complete,ready,seg,bbn,crs,regions,unsupported,duplicates=vals
 if not 1<=tables<=9 or ssdts>8 or tables!=ssdts+1 or objects>128:
  return False,{}
 if any(v>1 for v in (complete,ready,seg,bbn,crs)) or regions>1024 or unsupported>16384 or duplicates>128:
  return False,{}
 if ready and (not complete or not all((seg,bbn,crs)) or regions or unsupported or duplicates):return False,{}
 if complete and (regions or unsupported or duplicates):return False,{}
 chunks=[x for x in records if x.startswith('STYLEOS S816 SSDT')]
 originals=[];index=0;current=None;expected=None
 for line in chunks:
  match=HEADER.fullmatch(line)
  if match:
   if current is not None:
    if len(current)!=expected:return False,{}
    originals.append(bytes(current))
   ident,size=map(lambda v:int(v,16),match.groups())
   if ident!=index or not 36<=size<=24576:return False,{}
   current=bytearray();expected=size;index+=1;continue
  match=CHUNK.fullmatch(line)
  if not match or current is None:return False,{}
  ident,offset=int(match[1],16),int(match[2],16)
  if ident!=index-1 or offset!=len(current):return False,{}
  data=bytes.fromhex(match[3]);n=min(32,expected-offset)
  if not n or len(data)!=n:return False,{}
  current.extend(data)
 if current is not None:
  if len(current)!=expected:return False,{}
  originals.append(bytes(current))
 # On multiple SSDTs originals include only last unless we track each header.
 if index!=ssdts:return False,{}
 if any(x[:4]!=b'SSDT' or int.from_bytes(x[4:8],'little')!=len(x) or sum(x)%256 for x in originals):return False,{}
 old=[]
 for line in records:
  if line==key[0] or line.startswith('STYLEOS S816 SSDT'):continue
  if line==POLICY:old.append(POLICY815);continue
  old.append(line.replace('STYLEOS S816 ', 'STYLEOS S815 ',1))
 forged=('\n'.join(old)+'\n').encode('ascii')
 ok,details=old_verify(forged,rc,mode)
 if not ok or details.get('ssdt_present_unresolved')!=ssdts:return False,{}
 details['aml_namespace_fully_evaluated']=False
 details['aml_literal_subset_complete']=bool(complete)
 details['aml_literal_hostbridge_ready']=bool(ready)
 details['aml_objects']=objects
 details['aml_regions_denied']=regions
 details['aml_unsupported']=unsupported
 details['ssdt_count_copied']=ssdts
 details['ssdt_sha256']=[hashlib.sha256(b).hexdigest() for b in originals]
 details['_original_ssdt_bytes']=originals
 return True,details
