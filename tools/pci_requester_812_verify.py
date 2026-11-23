"""Exact S812 hardware UART grammar. A PASS string without an actual VM is not enough."""
import re
HEX=r'0x([0-9a-f]{16})'
PLAT=re.compile(r'^STYLEOS S812 PLATFORM backend='+HEX+r' root-entries='+HEX+'$')
TOPO=re.compile(r'^STYLEOS S812 TOPOLOGY buses='+HEX+r' functions='+HEX+r' bridges='+HEX+'$')
SCOPE=re.compile(r'^STYLEOS S812 SCOPE units='+HEX+r' exact='+HEX+r' unresolved='+HEX+
 r' bridge-scopes='+HEX+r' aliases='+HEX+r' extended='+HEX+r' include-all='+HEX+
 r' exceptions='+HEX+r' segments-unresolved='+HEX+r' collisions='+HEX+'$')
PCI=re.compile(r'^STYLEOS S812 PCI explicit-matches='+HEX+r' unresolved='+HEX+
 r' alias-sources-present='+HEX+'$')
POLICY=('STYLEOS S812 POLICY pci-bridge-walk=bounded multihop-intel=topology-checked '
 'amd-alias=not-ownership exceptions=not-grants segments=zero-only '
 'device-ownership-complete=no dma-domains-verified=no dma-access-authorized=no '
 'dma-isolation-verified=no production-cr3=deny')
def verify(raw:bytes,rc:int|None,mode:str):
 if rc!=33 or not raw or len(raw)>120000 or not raw.endswith(b'\n') or b'\0' in raw:return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 if any('STYLEOS S812 FAIL' in line for line in lines):return False,{}
 records=[line for line in lines if line.startswith('STYLEOS S812 ')]
 if len(records)!=6 or records[-2:]!=[POLICY,'STYLEOS S812 PASS']:return False,{}
 m=[p.fullmatch(line) for p,line in zip((PLAT,TOPO,SCOPE,PCI),records)]
 if not all(m):return False,{}
 backend,roots=(int(x,16) for x in m[0].groups())
 buses,functions,bridges=(int(x,16) for x in m[1].groups())
 units,exact,unresolved,bridge_scopes,aliases,extended,includes,exceptions,segments,collisions=(int(x,16) for x in m[2].groups())
 matched,unknown,alias_present=(int(x,16) for x in m[3].groups())
 if backend!={'disabled':0,'intel':1,'amd':2,'bridge-intel':1,'bridge-two-hop-intel':1}.get(mode):return False,{}
 if not 1<=roots<=64 or not 1<=buses<=256 or not 1<=functions<=4096:return False,{}
 if bridges>buses-1 or buses!=bridges+1:return False,{}
 if any((units>16,exact>256,aliases>256,extended>1024,unresolved>1024,
         bridge_scopes>1024,includes>16,exceptions>1024,segments>16,collisions!=0)):return False,{}
 if matched+unknown!=functions or alias_present>functions or matched>exact:return False,{}
 if mode=='disabled' and (units or exact or unresolved or bridge_scopes or aliases or
                           extended or includes or exceptions or segments or matched):return False,{}
 if mode!='disabled' and units==0:return False,{}
 if mode=='bridge-intel' and (buses<2 or bridges<1):return False,{}
 if mode=='bridge-two-hop-intel' and (buses<3 or bridges<2):return False,{}
 return True,{'backend':mode,'root_entries':roots,'buses':buses,'functions':functions,
  'bridges':bridges,'units':units,'explicit_scopes':exact,'explicit_matches':matched,
  'unknown_live_requesters':unknown,'bridge_scopes':bridge_scopes,'aliases':aliases,
  'aliases_on_live_functions':alias_present,'extended_records':extended,
  'include_all':includes,'memory_exceptions':exceptions,'nonzero_segments':segments,
  'bridge_walk_observed':mode in ('bridge-intel','bridge-two-hop-intel'),'global_hierarchy_verified':False,
  'device_ownership_complete':False,'iommu_domains_verified':False,
  'dma_isolation_verified':False,'dma_access_authorized':False,
  'production_cr3_authorized':False}
