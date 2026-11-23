"""S818 strict UART verifier: guest-denied ACPICA OSL substrate; inherited S816 evidence."""
import re
from acpi_namespace_816_verify import verify as verify816
H = r'0x([0-9a-f]{16})'
OSL = re.compile(r'^STYLEOS S818 OSL dsdt-bytes='+H+r' snapshot-reads='+H+
                 r' region-denied='+H+r' budget='+H+'$')
POLICY = ('STYLEOS S818 POLICY acpica-linked=no acpi-table=private-copy '
          'operationregion=deny-all hardware-io=none ecam=not-read dma-grants=no '
          'acpi-namespace-evaluated=no production-cr3=deny')
def verify(raw:bytes, rc:int|None, mode:str):
    good, data = verify816(raw, rc, mode)
    if not good or not isinstance(raw,bytes) or raw.count(b'STYLEOS S818 ') != 3:
        return False, {}
    try: lines=raw.decode('ascii').replace('\r','').splitlines()
    except UnicodeDecodeError:return False,{}
    entries=[line for line in lines if line.startswith('STYLEOS S818 ')]
    if len(entries)!=3 or entries[1:]!=[POLICY,'STYLEOS S818 PASS']:
        return False,{}
    m=OSL.fullmatch(entries[0]);
    if not m:return False,{}
    length, copied, denied, budget=[int(x,16) for x in m.groups()]
    if not (36<=length<=24576 and length==data.get('dsdt_bytes') and
            copied==1 and denied==12 and budget==256):return False,{}
    if lines.index(entries[2])>=next((i for i,s in enumerate(lines)
        if s.startswith('STYLEOS S816 PLATFORM')),len(lines)):
        return False,{}
    data.update({'osl818_region_denied':denied,'osl818_snapshot_reads':copied,
                 'osl818_acpica_linked':False,'osl818_hardware_io':False,
                 'osl818_dma_access_authorized':False,'osl818_cr3_authorized':False})
    return True,data
