#!/usr/bin/env python3
"""Strict real UART verifier: DMAR discovery != DMA isolation."""
import re
HEX=r'0x[0-9a-f]{16}'
def fail(msg):return False,{'reason':msg}
def parse_fields(line,prefix,names):
 pattern='^'+re.escape(prefix)+''.join(r' '+re.escape(n)+r'=('+HEX+r')' for n in names)+'$'
 m=re.fullmatch(pattern,line)
 if not m:return None
 return {n:int(x,16) for n,x in zip(names,m.groups())}
def verify(raw,returncode,expect_present):
 if returncode!=33:return fail('QEMU debug-exit did not return 33')
 if not isinstance(raw,(bytes,bytearray)) or len(raw)>100000:return fail('invalid UART length')
 try: data=raw.decode('ascii')
 except UnicodeDecodeError:return fail('invalid UART encoding')
 lines=[s.strip() for s in data.replace('\r','').split('\n') if s.strip()]
 s=[line for line in lines if line.startswith('STYLEOS S808')]
 if any('FAIL' in line for line in lines) or s.count('STYLEOS S808 PASS')!=1:return fail('missing PASS or failure marker')
 if len(s)<3 or s[-1]!='STYLEOS S808 PASS':return fail('wrong S808 record order')
 status=parse_fields(s[0],'STYLEOS S808 STATUS',['present','root-entries'])
 if not status or status['root-entries']<1 or status['root-entries']>64:return fail('status/root count invalid')
 if status['present'] not in (0,1) or bool(status['present']) != bool(expect_present):return fail('IOMMU configuration or DMAR discovery disagrees')
 policy='STYLEOS S808 POLICY dmar-table-verified=yes iommu-registers-observed=no iommu-translation-enabled=no dma-isolation-verified=no dma-access-authorized=no uefi-runtime-verified=no production-cr3=deny'
 if s[-2]!=policy:return fail('unsafe DMA or CR3 authorization')
 i=1;topology=None;drhds=[];rmrrs=[]
 if status['present']:
  if len(s)<4:return fail('missing DMAR header')
  topology=parse_fields(s[i],'STYLEOS S808 DMAR',['length','revision','host-aw','flags','drhd-count','rmrr-count','unknown-count']);i+=1
  if (not topology or topology['length']<48 or topology['length']>4096 or not 32<=topology['host-aw']<=63
      or topology['flags'] & ~3 or not 1<=topology['drhd-count']<=16 or topology['rmrr-count']>16):
   return fail('invalid DMAR header')
  for k in range(topology['drhd-count']):
   if i>=len(s)-2:return fail('missing DRHD')
   v=parse_fields(s[i],'STYLEOS S808 DRHD',['index','segment','flags','base','scope-count']);i+=1
   if (not v or v['index']!=k or v['segment']!=0 or v['flags']&~1 or v['base']==0 or v['base']%4096
       or v['scope-count']>64 or (v['flags']==0 and v['scope-count']==0)):
    return fail('bad DRHD')
   if any(x['base']==v['base'] for x in drhds):return fail('duplicate DRHD base')
   drhds.append(v)
  for k in range(topology['rmrr-count']):
   if i>=len(s)-2:return fail('missing RMRR')
   v=parse_fields(s[i],'STYLEOS S808 RMRR',['index','segment','base','limit','scope-count']);i+=1
   if not v or v['index']!=k or v['segment']!=0 or v['base']==0 or v['base']>v['limit'] or not 1<=v['scope-count']<=64:return fail('bad RMRR')
   if any(not(v['limit']<x['base'] or v['base']>x['limit']) for x in rmrrs):return fail('overlapping RMRR')
   rmrrs.append(v)
 if i!=len(s)-2:return fail('extra, missing, or duplicate S808 entries')
 return True,{'present':bool(status['present']),'root_entries':status['root-entries'],
  'dmar':topology,'drhd':drhds,'rmrr':rmrrs}
