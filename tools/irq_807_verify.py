"""Strict evidence gate for a REAL PIT0/GSI2/IOAPIC/LAPIC IRQ delivery.
Only the dedicated hardware-runner can supply the real QEMU exit and UART.
"""
import re
PAT=re.compile(rb"([a-z0-9-]+)=0x([0-9a-f]{16})")
FIELDS={
 'TOPOLOGY':('ioapic-physical','ioapic-id','ioapic-count','gsi','pin','lapic-physical','lapic-id'),
 'BEFORE':('ioapic-low','ioapic-high','tpr','svr','if'),
 'ROUTE':('vector','ioapic-low','ioapic-high','gsi'),
 'DELIVERY':('hits','isr-seen','eoi-cleared','elapsed-tsc'),
 'RESTORE':('ioapic-low','ioapic-high','tpr','svr','if'),
}
POLICY=b'STYLEOS S807 POLICY madt=verified ioapic-route=verified pit-irq0-delivery=verified lapic-isr-eoi=verified ioapic-lapic-restored=yes pit-restored=no ordinary-desktop=unchanged dma-iommu=unverified uefi-runtime=unverified production-cr3=deny'
def parse(line,kind):
 p=('STYLEOS S807 '+kind+' ').encode()
 if not line.startswith(p):raise ValueError('missing '+kind)
 raw=line[len(p):];matches=PAT.findall(raw)
 if tuple(k.decode() for k,v in matches)!=FIELDS[kind] or raw!=b' '.join(k+b'=0x'+v for k,v in matches):raise ValueError('bad '+kind+' fields')
 return {k.decode():int(v,16) for k,v in matches}
def verify(uart,exitcode):
 try:
  if not isinstance(uart,bytes) or not 0<len(uart)<500000 or exitcode!=33:raise ValueError('real QEMU debug-exit required')
  lines=uart.splitlines();prefix=b'STYLEOS S807 '
  marker=b'STYLEOS x86_64 ExitBootServices OK'
  if lines.count(marker)!=1 or lines.count(b'STYLEOS S807 PASS')!=1:raise ValueError('missing unique handoff or PASS')
  records=[x for x in lines if x.startswith(prefix)]
  if len(records)!=7 or records[-2:]!=[POLICY,b'STYLEOS S807 PASS'] or any(b'STYLEOS S807 FAIL' in x for x in lines):raise ValueError('bad PASS or extra evidence lines')
  if lines.index(marker)>=lines.index(records[0]):raise ValueError('premature hardware evidence')
  a,b,c,d,e=[parse(records[i],key) for i,key in enumerate(('TOPOLOGY','BEFORE','ROUTE','DELIVERY','RESTORE'))]
  if a['ioapic-physical']==0 or a['ioapic-physical']&4095 or a['lapic-physical']&4095 or a['lapic-physical']==0:raise ValueError('IOAPIC LAPIC MMIO base')
  if a['ioapic-id']>15 or a['lapic-id']>255 or not 1<=a['ioapic-count']<=120 or a['gsi']>65535 or a['pin']>=a['ioapic-count']:raise ValueError('invalid topology')
  if b['if']!=0 or e['if']!=0:raise ValueError('unexpected IF')
  if c['vector']!=0x50 or c['gsi']!=a['gsi'] or c['ioapic-low']!=0x50 or c['ioapic-high']!=a['lapic-id']<<24:raise ValueError('route mismatch')
  if e['ioapic-low']!=b['ioapic-low'] or e['ioapic-high']!=b['ioapic-high'] or e['tpr']!=b['tpr'] or e['svr']!=b['svr']:raise ValueError('hardware state not restored')
  if not 1<=d['hits']<=10000 or d['isr-seen']!=1 or d['eoi-cleared']!=1 or not 0<d['elapsed-tsc']<10_000_000_000:raise ValueError('IRQ/ISR/EOI not proven')
  return True,{'topology':a,'before':b,'route':c,'delivery':d,'restore':e,'irq_delivery_verified':True,'ioapic_redirection_verified':True,'lapic_eoi_verified':True,'pit_state_restored':False,'dma_iommu_verified':False,'uefi_runtime_verified':False,'production_cr3_authorized':False}
 except (ValueError,IndexError,KeyError,TypeError) as exc:return False,str(exc)
