"""Stage 823: strict guest ACPICA UART proof for *real* firmware host bridge.

Read-only AML is not PCI/ECAM/IOMMU ownership or DMA permission.
"""
import re
from acpi_namespace_816_verify import verify as verify816
H=r'0x([0-9a-f]{16})'
FW=re.compile(r'^STYLEOS S823 FIRMWARE dsdt-bytes='+H+r' ssdt-count='+H+
              r' image-owned-snapshot=yes original-aml=yes$')
HOST=re.compile(r'^STYLEOS S823 HOSTBRIDGE hid='+H+r' crs-bytes='+H+
                r' seg-present='+H+r' bbn-present='+H+
                r' real-AcpiEvaluateObject=yes$')
POLICY=('STYLEOS S823 POLICY firmware-methods=limited region=deny-all '
        'pci=deny-all dma-grants=no ecam-reservation=no '
        'production-cr3=deny foundation-complete=no')
OLD=('STYLEOS S821 POLICY upstream-real-code=yes full-core-compiled=yes '
     'aml-executed=no operationregion=deny-all dma-grants=no production-cr3=deny')

def verify(raw,rc,mode):
 if not isinstance(raw,bytes) or len(raw)>1100000 or b'\x00' in raw:return False,{}
 good,d=verify816(raw,rc,mode)
 if not good:return False,{}
 try:lines=raw.decode('ascii').replace('\r','').splitlines()
 except UnicodeDecodeError:return False,{}
 s=[x for x in lines if x.startswith('STYLEOS S823 ')]
 p=[x for x in lines if x.startswith('STYLEOS S821 ACPICA-CHECKSUM') or x in (OLD,'STYLEOS S821 PASS')]
 if len(s)!=4 or s[2:]!=[POLICY,'STYLEOS S823 PASS'] or len(p)!=3:return False,{}
 if p[1:]!=[OLD,'STYLEOS S821 PASS'] or not p[0].startswith('STYLEOS S821 ACPICA-CHECKSUM length='):return False,{}
 a=FW.fullmatch(s[0]);b=HOST.fullmatch(s[1])
 if not a or not b:return False,{}
 size,count=int(a[1],16),int(a[2],16)
 hid,crs,seg,bbn=[int(v,16) for v in b.groups()]
 if size<37 or size>24576 or count>8 or hid not in (0x080ad041,0x030ad041):return False,{}
 if crs<4 or crs>1024 or seg not in (0,1) or bbn not in (0,1):return False,{}
 if size!=d.get('dsdt_bytes'):return False,{}
 if any('dma-grants=yes' in x or x.startswith('STYLEOS S823 FAIL') for x in lines):return False,{}
 try:
  i=next(i for i,x in enumerate(lines) if x.startswith('STYLEOS S816 PLATFORM'))
  if not(lines.index('STYLEOS x86_64 ExitBootServices OK')<lines.index(p[0])<
         lines.index(p[2])<lines.index(s[0])<lines.index(s[3])<i):return False,{}
 except ValueError:return False,{}
 d.update({'acpica823_firmware_hid_evaluated':True,
           'acpica823_firmware_crs_evaluated':True,
           'acpica823_firmware_seg_present':bool(seg),
           'acpica823_firmware_bbn_present':bool(bbn),
           'acpica823_crs_bytes':crs,
           'acpica823_hid':hid,
           'acpica823_firmware_dsdt_bytes':size,
           'acpica823_firmware_ssdt_count':count,
           'acpica823_ecam_reserved':False,
           'acpica823_dma_authorized':False})
 return True,d
