"""Verify real 815 hardware record plus independently reassembled AML DSDT.

Exit code 33 and valid checksums are provenance evidence only,
not proof of ACPI namespace execution or authorization of DMA.
"""
import re
HEX=r'0x([0-9a-f]{16})'
PLATFORM=re.compile(r'^STYLEOS S815 PLATFORM backend='+HEX+r' root-entries='+HEX+'$')
TOPOLOGY=re.compile(r'^STYLEOS S815 TOPOLOGY buses='+HEX+r' functions='+HEX+r' bridges='+HEX+'$')
MCFG=re.compile(r'^STYLEOS S815 MCFG allocations='+HEX+r' segment0='+HEX+r' other-segments='+HEX+
 r' covered-buses='+HEX+r' missing-buses='+HEX+r' root-base='+HEX+r' root-end='+HEX+'$')
SCOPE=re.compile(r'^STYLEOS S815 SCOPE units='+HEX+r' exact='+HEX+r' unresolved='+HEX+
 r' aliases='+HEX+r' exceptions='+HEX+r' segments-unresolved='+HEX+r' collisions='+HEX+'$')
PCI=re.compile(r'^STYLEOS S815 PCI explicit-matches='+HEX+r' unresolved='+HEX+r' alias-sources-present='+HEX+'$')
ROOT=re.compile(r'^STYLEOS S815 ROOTBRIDGE roots='+HEX+r' segment0='+HEX+
 r' other-segments='+HEX+r' bus-ranges='+HEX+r' covered-buses='+HEX+r' unresolved-buses='+HEX+'$')
AML=re.compile(r'^STYLEOS S815 AML dsdt-bytes='+HEX+r' devices='+HEX+r' pci-hid='+HEX+
 r' motherboard-hid='+HEX+r' seg-static='+HEX+r' bbn-static='+HEX+
 r' crs-static='+HEX+r' crs-method='+HEX+r' ecam-literal='+HEX+r' ssdts='+HEX+'$')
CHUNK=re.compile(r'^STYLEOS S815 DSDTHEX offset='+HEX+r' data=([0-9a-f]{2,64})$')
POLICY=('STYLEOS S815 POLICY acpi-dsdt=checksummed acpi-aml=static-hints-only '
 'acpi-crs-evaluated=no acpi-seg-evaluated=no acpi-bbn-evaluated=no '
 'ecam-reservation-verified=no ecam-dereference=no iommu-ownership-complete=no '
 'dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny')
BACKEND={'disabled':0,'intel':1,'bridge-intel':1,'bridge-two-hop-intel':1,'amd':2}
def verify(raw:bytes,exit_code:int|None,mode:str):
 if exit_code!=33 or mode not in BACKEND or not raw or len(raw)>240000 or b'\x00' in raw or not raw.endswith(b'\n'):
  return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 if any('STYLEOS S815 FAIL' in ln for ln in lines):return False,{}
 rec=[ln for ln in lines if ln.startswith('STYLEOS S815 ')]
 if len(rec)<11 or rec[-2:]!=[POLICY,'STYLEOS S815 PASS']:return False,{}
 matches=[p.fullmatch(rec[i]) for i,p in enumerate((PLATFORM,TOPOLOGY,MCFG,SCOPE,PCI,ROOT,AML))]
 if not all(matches):return False,{}
 backend,roots=map(lambda x:int(x,16),matches[0].groups())
 buses,functions,bridges=map(lambda x:int(x,16),matches[1].groups())
 allocations,seg0,other,covered,missing,base,end=map(lambda x:int(x,16),matches[2].groups())
 units,exact,unresolved,aliases,exceptions,nonzero,collisions=map(lambda x:int(x,16),matches[3].groups())
 matched,unknown,alias_present=map(lambda x:int(x,16),matches[4].groups())
 root_count,rb0,rbother,busranges,rbcovered,rbmissing=map(lambda x:int(x,16),matches[5].groups())
 dsdt_size,devices,pci_hids,board_hids,segs,bbns,static_crs,method_crs,literals,ssdts=map(lambda x:int(x,16),matches[6].groups())
 if backend!=BACKEND[mode] or not 1<=roots<=64:return False,{}
 if not 1<=buses<=256 or not 1<=functions<=4096 or buses!=bridges+1:return False,{}
 if mode=='bridge-intel' and bridges<1:return False,{}
 if mode=='bridge-two-hop-intel' and bridges<2:return False,{}
 if not 1<=allocations<=32 or not 1<=seg0<=32 or other>32 or covered!=buses or missing:return False,{}
 if base>=end or base%1048576 or end-base!=1048576:return False,{}
 if not 1<=root_count<=16 or rb0!=1 or rbother>15 or not 1<=busranges<=24 or rbcovered!=buses or rbmissing:return False,{}
 if units>16 or exact>256 or unresolved>1024 or aliases>256 or exceptions>1024 or nonzero>16 or collisions:return False,{}
 if matched+unknown!=functions or matched>exact or alias_present>functions:return False,{}
 if mode=='disabled' and (units or exact or matched):return False,{}
 if mode!='disabled' and units==0:return False,{}
 if not 37<=dsdt_size<=24576 or devices>512 or pci_hids>512 or board_hids>512 or ssdts>64:return False,{}
 if segs>512 or bbns>512 or static_crs>512 or method_crs>512 or literals>512:return False,{}
 chunks=rec[7:-2]
 if len(chunks)!=(dsdt_size+31)//32:return False,{}
 data=bytearray()
 for ln in chunks:
  m=CHUNK.fullmatch(ln)
  if not m:return False,{}
  if int(m[1],16)!=len(data):return False,{}
  b=bytes.fromhex(m[2]);expected=min(32,dsdt_size-len(data))
  if len(b)!=expected:return False,{}
  data.extend(b)
 if len(data)!=dsdt_size or data[:4]!=b'DSDT' or int.from_bytes(data[4:8],'little')!=dsdt_size or sum(data)%256:
  return False,{}
 return True,{'backend':mode,'root_entries':roots,'buses':buses,'functions':functions,
  'bridges':bridges,'uefi_roots':root_count,'mcfg_covered_buses':covered,
  'uefi_covered_buses':rbcovered,'dsdt_bytes':dsdt_size,'aml_device_candidates':devices,
  'aml_pci_hid_candidates':pci_hids,'aml_motherboard_hid_candidates':board_hids,
  'aml_segment_literal_candidates':segs,'aml_rootbus_literal_candidates':bbns,
  'aml_crs_literal_candidates':static_crs,'aml_crs_method_candidates':method_crs,
  'aml_ecam_reservation_literal_candidates':literals,'ssdt_present_unresolved':ssdts,
  'fadt_dsdt_table_checksums_valid':True,'acpi_namespace_evaluated':False,
  'acpi_crs_evaluated':False,'acpi_seg_evaluated':False,'acpi_bbn_evaluated':False,
  'ecam_reservation_verified':False,'iommu_ownership_complete':False,
  'dma_domains_verified':False,'dma_isolation_verified':False,
  'dma_access_authorized':False,'production_cr3_authorized':False,
  '_original_dsdt_bytes':bytes(data)}
