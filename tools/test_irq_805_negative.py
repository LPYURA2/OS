#!/usr/bin/env python3
"""Synthetic negative fixtures. These are NOT hardware evidence."""
from irq_805_verify import FIELDS,POLICY,verify
def record(kind,fields):
 return b'STYLEOS S805 '+kind.encode()+b' '+b' '.join(k.encode()+b'=0x'+format(fields[k],'016x').encode() for k in FIELDS[kind])
c={'cpuid-ecx':1<<21,'cpuid-edx':(1<<5)|(1<<9),'apic-base-msr':0xfee00900,
   'lapic-physical':0xfee00000,'apic-global':1,'x2-mode':0,'bsp':1}
i={'idtr-base':0x7fd000,'idtr-limit':4095,'rflags':2,'if-enabled':0,
   'pic-master':255,'pic-slave':255,'cr3':0x7801000}
base=b'\n'.join([b'STYLEOS x86_64 ExitBootServices OK',record('CPU',c),record('INTERRUPTS',i),POLICY,b'STYLEOS S805 PASS'])+b'\n'
assert verify(base,33)[0],verify(base,33)
def mutate(k,x,y):
 assert x in base, (k,x)
 return (k,base.replace(x,y,1),33)
cases=[
 ('wrong-exit',base,0),('missing-boot',base.replace(b'STYLEOS x86_64 ExitBootServices OK',b'broken'),33),
 ('duplicate-pass',base+b'STYLEOS S805 PASS\n',33),
 ('fake-irq-delivery',base.replace(b'irq-delivery-unverified=yes',b'irq-delivery-verified=yes'),33),
 ('fake-madt',base.replace(b'madt-unverified=yes',b'madt-unverified=no'),33),
 ('fake-production-cr3',base.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
 ('extra-record',base.replace(POLICY,b'STYLEOS S805 INJECT data\n'+POLICY),33),
 ('missing-pass',base.replace(b'STYLEOS S805 PASS\n',b''),33),
 ('fail-alongside-pass',base+b'STYLEOS S805 FAIL corrupted\n',33),
 mutate('no-apic',b'cpuid-edx=0x0000000000000220',b'cpuid-edx=0x0000000000000020'),
 mutate('lapic-wrong',b'lapic-physical=0x00000000fee00000',b'lapic-physical=0x00000000fed00000'),
 mutate('apic-global',b'apic-global=0x0000000000000001',b'apic-global=0x0000000000000000'),
 mutate('if-enabled',b'if-enabled=0x0000000000000000',b'if-enabled=0x0000000000000001'),
 mutate('pic-too-large',b'pic-master=0x00000000000000ff',b'pic-master=0x00000000000001ff'),
 mutate('idtr-null',b'idtr-base=0x00000000007fd000',b'idtr-base=0x0000000000000000'),
 mutate('cr3-null',b'cr3=0x0000000007801000',b'cr3=0x0000000000000000'),
 mutate('x2-wrong',b'x2-mode=0x0000000000000000',b'x2-mode=0x0000000000000001'),
 mutate('numeric-format',b'lapic-physical=0x00000000fee00000',b'lapic-physical=0xFEE00000'),
]
for name,uart,rc in cases:
 if verify(uart,rc)[0]:raise AssertionError('S805 unsafe acceptance '+name)
print('STYLEOS S805 negative UART PASS',len(cases))
