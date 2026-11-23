#!/usr/bin/env python3
"""Refuse counterfeited real-firmware ACPICA _HID/_CRS proof."""
import argparse
from pathlib import Path
from acpica_firmware_823_verify import verify,POLICY
H=lambda n:'0x%016x'%n
STAGE=(f'STYLEOS S823 FIRMWARE dsdt-bytes={H(8390)} ssdt-count={H(0)} '
       'image-owned-snapshot=yes original-aml=yes\n'
       f'STYLEOS S823 HOSTBRIDGE hid={H(0x080ad041)} crs-bytes={H(31)} '
       f'seg-present={H(0)} bbn-present={H(0)} real-AcpiEvaluateObject=yes\n'
       f'{POLICY}\nSTYLEOS S823 PASS\n')

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--parent-uart',type=Path,required=True)
 base=ap.parse_args().parent_uart.read_bytes().replace(b'\r',b'')
 anchor=b'STYLEOS S821 PASS\n'
 assert base.count(anchor)==1
 good=base.replace(anchor,anchor+STAGE.encode('ascii'))
 assert verify(good,33,'disabled')[0], 'valid fixture rejected'
 bad=[
  ('original-aml=yes','original-aml=no'),
  ('image-owned-snapshot=yes','image-owned-snapshot=no'),
  ('real-AcpiEvaluateObject=yes','real-AcpiEvaluateObject=no'),
  (f'hid={H(0x080ad041)}', f'hid={H(42)}'),
  (f'crs-bytes={H(31)}',f'crs-bytes={H(0)}'),
  (f'crs-bytes={H(31)}',f'crs-bytes={H(2048)}'),
  (f'dsdt-bytes={H(8390)}',f'dsdt-bytes={H(8391)}'),
  (f'ssdt-count={H(0)}',f'ssdt-count={H(9)}'),
  ('region=deny-all','region=allow'),
  ('pci=deny-all','pci=allow'),
  ('dma-grants=no','dma-grants=yes'),
  ('ecam-reservation=no','ecam-reservation=yes'),
  ('production-cr3=deny','production-cr3=permit'),
  ('foundation-complete=no','foundation-complete=yes'),
  ('firmware-methods=limited','firmware-methods=all'),
  ('STYLEOS S823 PASS','STYLEOS S823 FAIL'),
  ('STYLEOS S823 PASS\n',''),
  ('STYLEOS S823 HOSTBRIDGE','STYLEOS S823 FORGED'),
  ('STYLEOS S823 POLICY','STYLEOS S823 POLICY spoof=1'),
  ('STYLEOS x86_64 ExitBootServices OK','STYLEOS EBS FAILED'),
  ('STYLEOS S816 PASS','STYLEOS S816 FORGED'),
 ]
 count=0
 for i,(a,b) in enumerate(bad):
  changed=good.replace(a.encode(),b.encode())
  assert changed!=good,(i,a)
  assert not verify(changed,33,'disabled')[0],(i,a)
  count+=1
 for v in [good+b'\x00',good[:500],good+STAGE.encode(),b'',good.replace(anchor,b'')]:
  assert not verify(v,33,'disabled')[0];count+=1
 for c in [None,0,32,34]:
  assert not verify(good,c,'disabled')[0];count+=1
 print(f'STYLEOS S823 NEGATIVE {count}/{count} PASS')
if __name__=='__main__':main()
