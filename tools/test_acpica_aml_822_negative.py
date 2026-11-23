#!/usr/bin/env python3
"""Reject forged guest AML, DMA and missing table/physical evidence."""
import argparse
from pathlib import Path
from acpica_aml_822_verify import verify, POLICY
H=lambda v:'0x%016x'%v
STAGE=(f'STYLEOS S822 AML DSDT-T822={H(0x822)} SSDT-S822={H(0x2345)} real-AcpiEvaluateObject=yes memory-owned=yes\n'
       f'STYLEOS S822 OSL maps={H(12)} denied-maps={H(0)} heap-bytes={H(65536)}\n'
       f'{POLICY}\nSTYLEOS S822 PASS\n')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--parent-uart',type=Path,required=True)
 a=ap.parse_args();base=a.parent_uart.read_bytes().replace(b'\r',b'')
 anchor=b'STYLEOS S821 PASS\n'
 assert base.count(anchor)==1
 good=base.replace(anchor,anchor+STAGE.encode())
 assert verify(good,33,'disabled')[0],'valid parent fixture with S822 records rejected'
 cases=[
  (b'real-AcpiEvaluateObject=yes',b'real-AcpiEvaluateObject=no'),
  (b'DSDT-T822='+H(0x822).encode(),b'DSDT-T822='+H(0x823).encode()),
  (b'SSDT-S822='+H(0x2345).encode(),b'SSDT-S822='+H(0x2344).encode()),
  (b'memory-owned=yes',b'memory-owned=no'),
  (b'synthetic-only=yes',b'synthetic-only=no'),
  (b'real-firmware-methods=no',b'real-firmware-methods=yes'),
  (b'operationregion=deny-all',b'operationregion=permit'),
  (b'pci-io=deny-all',b'pci-io=permit'),
  (b'dma-grants=no',b'dma-grants=yes'),
  (b'production-cr3=deny',b'production-cr3=permit'),
  (b'foundation-complete=no',b'foundation-complete=yes'),
  (b'STYLEOS S822 PASS',b'STYLEOS S822 FAIL'),
  (b'STYLEOS S822 PASS\n',b''),
  (anchor,b''),
  (b'STYLEOS S816 PASS\n',b''),
  (b'STYLEOS x86_64 ExitBootServices OK',b'STYLEOS x86_64 EBS FAILED'),
  (b'STYLEOS S822 AML',b'STYLEOS S822 FAKE'),
  (b'STYLEOS S822 POLICY ',b'STYLEOS S822 POLICY forged=yes '),
  (b'STYLEOS S822 OSL maps='+H(12).encode(),b'STYLEOS S822 OSL maps='+H(0).encode()),
  (b'heap-bytes='+H(65536).encode(),b'heap-bytes='+H(5*1024*1024).encode()),
  (b'STYLEOS S822 POLICY ',b'STYLEOS S822 FAIL '),
  (b'STYLEOS S816 NAMESPACE',b'STYLEOS S816 INVALID'),
 ]
 total=0
 for i,(a,b) in enumerate(cases):
  changed=good.replace(a,b)
  assert changed!=good,(i,a)
  assert not verify(changed,33,'disabled')[0],f'forgery accepted {i}'
  total+=1
 for raw in [good+good.split(anchor)[1][:50],good+b'\x00',good[:300]]:
  assert not verify(raw,33,'disabled')[0];total+=1
 for rc in [0,32,34,None]:
  assert not verify(good,rc,'disabled')[0];total+=1
 print(f'STYLEOS S822 NEGATIVE {total}/{total} PASS')
if __name__=='__main__':main()
