#!/usr/bin/env python3
"""Strict UART parser for live paging/PAT sample. Real QEMU exit is required.

A valid synthetic fixture passes parser tests, but is NOT hardware proof.
All MMIO alias and production cache claims remain explicitly unverified.
"""
import re
KEYS={
 'CPU':('cr0','cr3','pat','physbits'),
 'TARGET':('index','address','present','translated','page-size','entry','pat-index','pat-type'),
}
POLICY=b'STYLEOS S801 POLICY live-pagewalk=observed global-alias-scan=unverified effective-mmio-cache=unverified production-cr3=deny'
TOKEN=re.compile(rb'([a-z][a-z0-9-]*)=0x([0-9a-f]{16})')
ADDRESS=[0x80000000,0x81000000,0xc000000000]
def parse(line,kind):
 prefix=b'STYLEOS S801 '+kind.encode()+b' '
 if not line.startswith(prefix):raise ValueError('prefix')
 body=line[len(prefix):]
 matches=TOKEN.findall(body)
 if tuple(k.decode() for k,v in matches)!=KEYS[kind] or body!=b' '.join(k+b'=0x'+v for k,v in matches):
  raise ValueError('record format '+kind)
 return {k.decode():int(v,16) for k,v in matches}
def verify(data,rc):
 try:
  if not isinstance(data,bytes) or len(data)>1000000 or rc!=33:raise ValueError('QEMU exit')
  lines=data.splitlines()
  if lines.count(b'STYLEOS x86_64 ExitBootServices OK')!=1:raise ValueError('handoff')
  obs=[x for x in lines if x.startswith(b'STYLEOS S801 ')]
  if len(obs)!=6 or obs[-2:]!=[POLICY,b'STYLEOS S801 PASS']:
   raise ValueError('sequence/policy')
  cpu=parse(obs[0],'CPU')
  if not(cpu['cr0']&(1<<31) and cpu['cr3']&~4095 and 36<=cpu['physbits']<=52):
   raise ValueError('CPU paging')
  if any((cpu['pat']>>(n*8))&255 not in (0,1,4,5,6,7) for n in range(8)):
   raise ValueError('PAT reserved')
  targets=[];mapped=0
  for idx in range(3):
   row=parse(obs[idx+1],'TARGET')
   if row['index']!=idx or row['address']!=ADDRESS[idx]:raise ValueError('address')
   if row['present'] not in (0,1):raise ValueError('present encoding')
   if not row['present']:
    if any(row[k]!=0 for k in ('translated','page-size','entry','pat-index','pat-type')):
     raise ValueError('absent leaf fields')
   else:
    mapped+=1
    size=row['page-size'];ent=row['entry'];virt=row['address']
    if size not in (4096,2097152,1073741824) or not ent&1:raise ValueError('present leaf')
    shift={4096:12,2097152:21,1073741824:30}[size]
    if size!=4096 and not(ent&128):raise ValueError('huge leaf PS')
    allowed=((1<<cpu['physbits'])-1)&~4095
    mask=allowed&~(size-1)
    forbidden=(size-1)&~4095
    if size>4096:forbidden&=~4096
    if ent&forbidden:raise ValueError('reserved page bits')
    actual=(ent&mask)+(virt&(size-1))
    idxpat=(((ent>>(7 if size==4096 else 12))&1)<<2)|(((ent>>4)&1)<<1)|((ent>>3)&1)
    selected=(cpu['pat']>>(8*idxpat))&255
    if (row['translated'],row['pat-index'],row['pat-type'])!=(actual,idxpat,selected):
     raise ValueError('incorrect hardware leaf/PAT extraction')
   targets.append(row)
  if not mapped:raise ValueError('vacuous no sampled translations')
  if any(x.startswith(b'STYLEOS S801 FAIL') for x in lines):raise ValueError('fail evidence')
  if lines.index(b'STYLEOS x86_64 ExitBootServices OK')>=lines.index(obs[0]):raise ValueError('ordering')
  return True,{'cpu':cpu,'targets':targets,'mapped_sample_addresses':mapped,
               'all_mmio_aliases_excluded':False,'production_cr3_authorized':False,
               'effective_mmio_cache_verified':False}
 except (ValueError,IndexError,KeyError,TypeError) as e:return False,str(e)
