#!/usr/bin/env python3
"""Synthetic UART tests, not hardware evidence."""
from acpi_806_verify import verify,POLICY

def rec(kind,fields):
 return (b'STYLEOS S806 '+kind.encode()+b' '+b' '.join(k.encode()+b'=0x'+f'{v:016x}'.encode() for k,v in fields.items()))
root=rec('ROOT',{'revision':2,'xsdt-entries':5,'madt-length':74})
lapic=rec('LAPIC',{'physical':0xfee00000,'flags':1,'enabled-cpus':1})
io=rec('IOAPIC',{'index':0,'id':2,'physical':0xfec00000,'gsi-base':0})
iso=rec('OVERRIDE',{'index':0,'bus':0,'source':0,'gsi':2,'flags':0})
summary=rec('SUMMARY',{'ioapic-count':1,'override-count':1})
marker=b'STYLEOS x86_64 ExitBootServices OK'
base=b'\n'.join([marker,root,lapic,io,iso,summary,POLICY,b'STYLEOS S806 PASS'])+b'\n'
assert verify(base,33)[0],verify(base,33)
cases=[('exit',base,1),('missing EBS',base.replace(marker,b'MISSING EBS'),33),('duplicate-pass',base+b'STYLEOS S806 PASS\n',33),
 ('false-delivery',base.replace(b'irq-delivery-unverified=yes',b'irq-delivery-unverified=no'),33),
 ('false-CR3',base.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
 ('missing-ISO',base.replace(iso+b'\n',b''),33),('extra-ISO',base.replace(summary,iso+b'\n'+summary),33),
 ('bad-count',base.replace(b'ioapic-count=0x0000000000000001',b'ioapic-count=0x0000000000000002'),33),
 ('bad-LAPIC',base.replace(b'physical=0x00000000fee00000',b'physical=0x0000000000000000'),33),
 ('bad-ISO-source',base.replace(b'source=0x0000000000000000',b'source=0x0000000000000010'),33),
 ('bad-ISO-flags',base.replace(b'flags=0x0000000000000000',b'flags=0x0000000000000002'),33),
 ('bad-revision',base.replace(b'revision=0x0000000000000002',b'revision=0x0000000000000001'),33),
 ('bad-index',base.replace(b'index=0x0000000000000000',b'index=0x0000000000000001',1),33),
 ('fail-and-pass',base+b'STYLEOS S806 FAIL forged\n',33),
 ('extra-record',base.replace(POLICY,b'STYLEOS S806 UNKNOWN data\n'+POLICY),33),
 ('malformed-hex',base.replace(b'0x0000000000000002',b'0x2',1),33),
 ('duplicate-EBS',marker+b'\n'+base,33)]
for name,data,rc in cases:
 if verify(data,rc)[0]:raise AssertionError('false PASS: '+name)
print('STYLEOS S806 negative UART PASS',len(cases))
