#!/usr/bin/env python3
"""Reject counterfeits; register presence is never DMA isolation."""
import re
HEX=r'0x[0-9a-f]{16}'
def fields(line,tag,keys):
 pattern='^STYLEOS S809 '+re.escape(tag)+''.join(' '+re.escape(k)+'=('+HEX+')' for k in keys)+'$'
 m=re.fullmatch(pattern,line)
 return {k:int(v,16) for k,v in zip(keys,m.groups())} if m else None
def bad(reason):return False,{'reason':reason}
def verify(raw,rc,enabled):
 if rc!=33:return bad('QEMU did not exit 33')
 if not isinstance(raw,bytes) or len(raw)>100000:return bad('wrong UART size')
 try:lines=[x.strip() for x in raw.decode('ascii').replace('\r','').split('\n') if x.strip()]
 except UnicodeDecodeError:return bad('bad encoding')
 s=[x for x in lines if x.startswith('STYLEOS S809')]
 if any('FAIL' in x for x in lines) or s.count('STYLEOS S809 PASS')!=1:return bad('failure or absent PASS')
 policy='STYLEOS S809 POLICY register-snapshot=observed dma-isolation-verified=no dma-access-authorized=no iommu-domains-verified=no uefi-runtime-verified=no production-cr3=deny'
 if not s or s[-2:]!=[policy,'STYLEOS S809 PASS']:return bad('bad policy/pass')
 status=fields(s[0],'STATUS',['present','drhd-count'])
 if not status or status['present']!=int(enabled) or status['drhd-count']!=(1 if enabled else 0):return bad('wrong topology')
 if not enabled:
  return (True,{'present':False}) if len(s)==3 else bad('extra records with no IOMMU')
 if len(s)!=6:return bad('missing or duplicate registers')
 mem=fields(s[1],'MMIO',['base','mapped','physical','page-size','cr3'])
 r=fields(s[2],'REGS',['ver','major','minor','cap','ecap','gcmd','gsts','rtaddr','fsts'])
 state=fields(s[3],'STATE',['translation-status','translation-command','root-set-status'])
 if not mem or not r or not state:return bad('invalid register syntax')
 if mem['base']==0 or mem['base']!=mem['physical'] or mem['base']&4095 or mem['mapped']!=1:return bad('MMIO not identity-mapped')
 if mem['page-size'] not in (4096,2097152,1073741824) or mem['cr3']==0 or mem['cr3']&4095:return bad('bad CR3/page size')
 if r['ver']&~255 or r['major']==0 or r['major']!=(r['ver']>>4)&15 or r['minor']!=r['ver']&15:return bad('bad VT-d version')
 if r['cap'] in (0,2**64-1) or r['ecap']==2**64-1 or any(r[k]>2**32-1 for k in ('gcmd','gsts','fsts')):return bad('invalid capability registers')
 if state['translation-status']!=(r['gsts']>>31)&1 or state['translation-command']!=(r['gcmd']>>31)&1 or state['root-set-status']!=(r['gsts']>>30)&1:return bad('invalid status bits')
 return True,{'present':True,'mmio':mem,'registers':r,'state':state}
