"""S813: reject textual SUCCESS unless real QEMU exited 33 and every record fits."""
import re
HEX=r'0x([0-9a-f]{16})'
PLATFORM=re.compile(r'^STYLEOS S813 PLATFORM backend='+HEX+r' root-entries='+HEX+'$')
TOPOLOGY=re.compile(r'^STYLEOS S813 TOPOLOGY buses='+HEX+r' functions='+HEX+r' bridges='+HEX+'$')
MCFG=re.compile(r'^STYLEOS S813 MCFG allocations='+HEX+r' segment0='+HEX+
 r' other-segments='+HEX+r' covered-buses='+HEX+r' missing-buses='+HEX+
 r' root-base='+HEX+r' root-end='+HEX+'$')
SCOPE=re.compile(r'^STYLEOS S813 SCOPE units='+HEX+r' exact='+HEX+
 r' unresolved='+HEX+r' aliases='+HEX+r' exceptions='+HEX+
 r' segments-unresolved='+HEX+r' collisions='+HEX+'$')
PCI=re.compile(r'^STYLEOS S813 PCI explicit-matches='+HEX+
 r' unresolved='+HEX+r' alias-sources-present='+HEX+'$')
POLICY=('STYLEOS S813 POLICY mcfg-checked=yes ecam-dereference=no '
 'segment0-cf8-only=yes nonzero-segments-enumerated=no '
 'hostbridge-crs-verified=no iommu-ownership-complete=no '
 'dma-domains-verified=no dma-access-authorized=no '
 'dma-isolation-verified=no production-cr3=deny')
BACKEND={'disabled':0,'intel':1,'bridge-intel':1,'bridge-two-hop-intel':1,'amd':2}
def verify(raw:bytes,rc:int|None,mode:str):
 if rc!=33 or mode not in BACKEND or not raw or len(raw)>120000 or b'\0' in raw or not raw.endswith(b'\n'):
  return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 if any('STYLEOS S813 FAIL' in x for x in lines):return False,{}
 records=[x for x in lines if x.startswith('STYLEOS S813 ')]
 if len(records)!=7 or records[-2:]!=[POLICY,'STYLEOS S813 PASS']:return False,{}
 patterns=(PLATFORM,TOPOLOGY,MCFG,SCOPE,PCI)
 m=[pat.fullmatch(line) for pat,line in zip(patterns,records)]
 if not all(m):return False,{}
 backend,root=map(lambda a:int(a,16),m[0].groups())
 buses,functions,bridges=map(lambda a:int(a,16),m[1].groups())
 allocations,seg0,other,covered,missing,base,end=map(lambda a:int(a,16),m[2].groups())
 units,exact,unresolved,aliases,exceptions,nonzero,collisions=map(lambda a:int(a,16),m[3].groups())
 matched,unknown,alias_present=map(lambda a:int(a,16),m[4].groups())
 if backend!=BACKEND[mode] or not 1<=root<=64:return False,{}
 if not 1<=buses<=256 or not 1<=functions<=4096 or bridges!=buses-1:return False,{}
 if not 1<=allocations<=32 or not 1<=seg0<=32 or seg0+other!=allocations:return False,{}
 if covered!=buses or missing!=0 or base==0 or base%1048576 or end!=base+1048576:return False,{}
 if not 0<=units<=16 or not 0<=exact<=256 or not 0<=aliases<=256:return False,{}
 if unresolved>1024 or exceptions>1024 or nonzero>16 or collisions!=0:return False,{}
 if matched>exact or matched+unknown!=functions or alias_present>aliases or alias_present>functions:return False,{}
 if backend==0 and (units or exact or unresolved or aliases or exceptions or nonzero or matched):return False,{}
 if backend!=0 and not units:return False,{}
 if mode=='bridge-intel' and bridges<1:return False,{}
 if mode=='bridge-two-hop-intel' and bridges<2:return False,{}
 return True,{'backend':mode,'root_entries':root,'functions':functions,'bridges':bridges,
  'buses':buses,'mcfg_allocations':allocations,'segment0_allocations':seg0,
  'unprobed_other_segments':other,'firmware_covered_observed_buses':covered,
  'firmware_uncovered_observed_buses':missing,'firmware_root_ecam_base':base,
  'firmware_root_bus0_end':end,'iommu_units':units,'explicit_scopes':exact,
  'live_explicit_matches':matched,'unknown_live_requesters':unknown,
  'aliases':aliases,'exceptions':exceptions,'segment_claims_unresolved':nonzero,
  'ecam_memory_mapped':False,'ecam_registers_read':False,
  'full_pci_segments_verified':False,'host_bridge_crs_verified':False,
  'iommu_ownership_complete':False,'dma_domains_verified':False,
  'dma_access_authorized':False,'dma_isolation_verified':False,
  'production_cr3_authorized':False}
