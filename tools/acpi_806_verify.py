#!/usr/bin/env python3
"""Strict terminal UART verifier. A valid topology does not establish IRQ delivery."""
import re
HEX=re.compile(rb'([a-z0-9-]+)=0x([0-9a-f]{16})')
POLICY=b'STYLEOS S806 POLICY acpi-checksums=verified madt-topology=observed ioapic-redirection-unverified=yes irq-delivery-unverified=yes dma-iommu-unverified=yes uefi-runtime-unverified=yes production-cr3=deny'
FIELDS={'ROOT':('revision','xsdt-entries','madt-length'),
        'LAPIC':('physical','flags','enabled-cpus'),
        'IOAPIC':('index','id','physical','gsi-base'),
        'OVERRIDE':('index','bus','source','gsi','flags'),
        'SUMMARY':('ioapic-count','override-count')}
def parse(line,kind):
 prefix=('STYLEOS S806 '+kind+' ').encode()
 if not line.startswith(prefix):raise ValueError('unexpected '+kind)
 raw=line[len(prefix):];pairs=HEX.findall(raw)
 if tuple(k.decode() for k,v in pairs)!=FIELDS[kind] or raw!=b' '.join(k+b'=0x'+v for k,v in pairs):
  raise ValueError('invalid '+kind+' field format')
 return {k.decode():int(v,16) for k,v in pairs}
def verify(data,exit_code):
 try:
  if not isinstance(data,bytes) or len(data)>500000 or exit_code!=33:raise ValueError('QEMU debug-exit 33 required')
  lines=data.splitlines()
  marker=b'STYLEOS x86_64 ExitBootServices OK'
  if lines.count(marker)!=1:raise ValueError('exit boot marker absent/duplicate')
  records=[v for v in lines if v.startswith(b'STYLEOS S806 ')]
  if len(records)<5 or records[-2:]!=[POLICY,b'STYLEOS S806 PASS'] or any(b'STYLEOS S806 FAIL' in v for v in lines):
   raise ValueError('missing/extra records or nonconservative policy')
  if lines.index(marker)>=lines.index(records[0]):raise ValueError('not after EBS')
  root=parse(records[0],'ROOT');lapic=parse(records[1],'LAPIC')
  if root['revision']<2 or not 1<=root['xsdt-entries']<=128 or not 44<=root['madt-length']<=4096:
   raise ValueError('invalid ACPI root')
  if not lapic['physical'] or lapic['physical']&4095 or not 1<=lapic['enabled-cpus']<=256:
   raise ValueError('invalid LAPIC')
  summary=parse(records[-3],'SUMMARY')
  ic=summary['ioapic-count'];oc=summary['override-count']
  if not 1<=ic<=8 or oc>16 or len(records)!=5+ic+oc:raise ValueError('bad cardinality')
  io=[];iso=[]
  for x in range(ic):
   a=parse(records[2+x],'IOAPIC');
   if a['index']!=x or a['physical']==0 or a['physical']&4095 or a['id']>255 or a in io:
    raise ValueError('bad IOAPIC')
   if any(b['id']==a['id'] or b['physical']==a['physical'] or b['gsi-base']==a['gsi-base'] for b in io):raise ValueError('duplicate IOAPIC')
   io.append(a)
  for x in range(oc):
   a=parse(records[2+ic+x],'OVERRIDE');flags=a['flags']
   if a['index']!=x or a['bus']!=0 or a['source']>=16 or flags&0xfff0 or flags&3==2 or flags>>2&3==2 or any(b['source']==a['source'] for b in iso):
    raise ValueError('bad ISO')
   iso.append(a)
  return True,{'root':root,'lapic':lapic,'ioapic':io,'overrides':iso,
               'acpi_checksums_verified':True,'madt_topology_observed':True,
               'ioapic_redirection_verified':False,'irq_delivery_verified':False,
               'dma_iommu_verified':False,'uefi_runtime_verified':False,
               'production_cr3_authorized':False}
 except (ValueError,IndexError,KeyError,TypeError) as exc:return False,str(exc)
