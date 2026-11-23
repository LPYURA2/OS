"""Strict verifier of real S811 QEMU UART; no IOMMU/DMA security claims."""
import re
HEX=r'0x([0-9a-f]{16})'
PLAT=re.compile(r'^STYLEOS S811 PLATFORM backend='+HEX+r' root-entries='+HEX+'$')
SCOPE=re.compile(r'^STYLEOS S811 SCOPE units='+HEX+r' exact='+HEX+r' unresolved='+HEX+r' include-all='+HEX+r' ranges='+HEX+r' collisions='+HEX+'$')
PCI=re.compile(r'^STYLEOS S811 PCI bus='+HEX+r' present='+HEX+r' explicit-matches='+HEX+r' unresolved='+HEX+'$')
POLICY='STYLEOS S811 POLICY pci-bus0-only=yes bridge-hierarchy-verified=no device-ownership-complete=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny'
def verify(raw:bytes,exit_code:int|None,expected:str):
 if exit_code!=33 or not raw or not raw.endswith(b'\n') or b'\x00' in raw or len(raw)>120000:return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 if any('STYLEOS S811 FAIL' in s for s in lines):return False,{}
 records=[s for s in lines if s.startswith('STYLEOS S811 ')]
 if len(records)!=5 or records[-2:]!=[POLICY,'STYLEOS S811 PASS']:return False,{}
 ms=[PLAT.fullmatch(records[0]),SCOPE.fullmatch(records[1]),PCI.fullmatch(records[2])]
 if not all(ms):return False,{}
 backend,roots=map(lambda s:int(s,16),ms[0].groups())
 units,exact,unresolved,include_all,ranges,collision=map(lambda s:int(s,16),ms[1].groups())
 bus,present,matched,unknown=map(lambda s:int(s,16),ms[2].groups())
 if backend!={'disabled':0,'intel':1,'amd':2}.get(expected) or not 1<=roots<=64:return False,{}
 if units>16 or exact>256 or unresolved>1024 or include_all>16 or ranges>256 or collision!=0:return False,{}
 if not 1<=present<=256 or bus!=0 or matched>present or matched+unknown!=present:return False,{}
 if expected=='disabled' and (units or exact or unresolved or include_all or ranges or matched):return False,{}
 if expected!='disabled' and units==0:return False,{}
 return True,{'backend':expected,'root_entries':roots,'units':units,'explicit_scopes':exact,
   'unresolved_scopes':unresolved,'include_all_scopes':include_all,'bounded_ranges':ranges,
   'live_bus0_functions':present,'live_explicit_matches':matched,'live_unresolved_functions':unknown,
   'pci_bus0_observed':True,'complete_pci_hierarchy_verified':False,
   'iommu_domains_verified':False,'dma_access_authorized':False,'dma_isolation_verified':False,
   'production_cr3_authorized':False}
