"""Counterfeit S819 guest callback evidence must fail, including nested 818/816."""
from test_acpi_osl_818_negative import sample as old_sample
from acpica_bridge_819_verify import verify,POLICY
H=lambda n:f'0x{n:016x}'
def sample(mode):
    old=old_sample(mode)
    text=(f'STYLEOS S819 TABLES registered={H(1)} maps={H(1)} '
          f'denied-maps={H(1)} denied-hardware={H(6)} ssdts={H(0)}\n'
          +POLICY+'\nSTYLEOS S819 PASS\n')
    return old.replace(b'STYLEOS S816 PLATFORM',text.encode()+b'STYLEOS S816 PLATFORM',1)
count=0
for mode in ('disabled','intel','bridge-intel','bridge-two-hop-intel','amd'):
    raw=sample(mode)
    ok,detail=verify(raw,33,mode)
    assert ok,(mode,detail)
    fake=[(raw,0),(raw,1),(raw[:-1],33),
          (raw+b'STYLEOS S819 PASS\n',33),
          (raw.replace(b'STYLEOS S819 PASS',b'STYLEOS S819 FAIL'),33),
          (raw.replace(b'acpica-linked=no',b'acpica-linked=yes'),33),
          (raw.replace(b'acpi-mapping=checked-copy-only',b'acpi-mapping=all-physical'),33),
          (raw.replace(b'operationregion=deny-all',b'operationregion=allow-all'),33),
          (raw.replace(b'ecam=not-read',b'ecam=read'),33),
          (raw.replace(b'dma-grants=no',b'dma-grants=yes'),33),
          (raw.replace(b'production-cr3=deny',b'production-cr3=allow'),33),
          (raw.replace(f'denied-hardware={H(6)}'.encode(),f'denied-hardware={H(5)}'.encode()),33),
          (raw.replace(f'denied-hardware={H(6)}'.encode(),f'denied-hardware={H(7)}'.encode()),33),
          (raw.replace(f'denied-maps={H(1)}'.encode(),f'denied-maps={H(0)}'.encode()),33),
          (raw.replace(f'maps={H(1)}'.encode(),f'maps={H(2)}'.encode()),33),
          (raw.replace(f'registered={H(1)}'.encode(),f'registered={H(0)}'.encode()),33),
          (raw.replace(f'ssdts={H(0)}'.encode(),f'ssdts={H(1)}'.encode()),33),
          (raw.replace(POLICY.encode()+b'\n',b''),33),
          (raw.replace(b'STYLEOS S819 PASS\n',b'',1),33),
          (raw.replace(b'STYLEOS S818 POLICY',b'STYLEOS S818 NO_POLICY'),33),
          (raw.replace(b'STYLEOS S816 POLICY',b'STYLEOS S816 NO_POLICY'),33)]
    for mutated,code in fake:
        assert not verify(mutated,code,mode)[0],(mode,code,mutated[:400]);count+=1
print(f'STYLEOS S819 NEGATIVE PASS {count}/{count}')
