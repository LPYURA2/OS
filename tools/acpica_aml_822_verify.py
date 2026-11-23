"""S822 actual AcpiEvaluateObject UART evidence; synthetic-only table methods."""
import re
from acpi_namespace_816_verify import verify as verify816
H=r'0x([0-9a-f]{16})'
AML=re.compile(r'^STYLEOS S822 AML DSDT-T822='+H+r' SSDT-S822='+H+r' real-AcpiEvaluateObject=yes memory-owned=yes$')
OSL=re.compile(r'^STYLEOS S822 OSL maps='+H+r' denied-maps='+H+r' heap-bytes='+H+'$')
POLICY=('STYLEOS S822 POLICY synthetic-only=yes real-firmware-methods=no '
        'operationregion=deny-all pci-io=deny-all dma-grants=no '
        'production-cr3=deny foundation-complete=no')
OLD=('STYLEOS S821 POLICY upstream-real-code=yes full-core-compiled=yes '
     'aml-executed=no operationregion=deny-all dma-grants=no production-cr3=deny')
def verify(raw,rc,mode):
 if not isinstance(raw,bytes) or len(raw)>1100000 or b'\x00' in raw:return False,{}
 ok,data=verify816(raw,rc,mode)
 if not ok:return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 s=[x for x in lines if x.startswith('STYLEOS S822 ')]
 p=[x for x in lines if x.startswith('STYLEOS S821 ')]
 if len(s)!=4 or len(p)!=3 or s[2:]!=[POLICY,'STYLEOS S822 PASS']:return False,{}
 if p[1:]!=[OLD,'STYLEOS S821 PASS'] or not p[0].startswith('STYLEOS S821 ACPICA-CHECKSUM length='):return False,{}
 a=AML.fullmatch(s[0]);b=OSL.fullmatch(s[1])
 if not a or not b or int(a[1],16)!=0x822 or int(a[2],16)!=0x2345:return False,{}
 m,denied,heap=map(lambda x:int(x,16),b.groups())
 if m<3 or m>200 or denied>150 or heap<64 or heap>4*1024*1024:return False,{}
 try:
  i=next(i for i,x in enumerate(lines) if x.startswith('STYLEOS S816 NAMESPACE'))
  if not(lines.index('STYLEOS x86_64 ExitBootServices OK')<lines.index(p[0])<
         lines.index(p[2])<lines.index(s[0])<lines.index(s[3])<i):return False,{}
 except ValueError:return False,{}
 if any(x.startswith('STYLEOS S822 FAIL') or 'dma-grants=yes' in x for x in lines):return False,{}
 data.update({'acpica822_real_aml_evaluated':True,'synthetic_dsdt_result':0x822,
              'synthetic_ssdt_result':0x2345,'acpica822_accepted_maps':m,
              'acpica822_denied_maps':denied,'acpica822_heap_bytes':heap,
              'acpica822_real_firmware_methods_evaluated':False,
              'acpica822_dma_authorized':False})
 return True,data
