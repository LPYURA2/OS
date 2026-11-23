"""Exact S819 UART evidence: ACPICA-style guest callbacks, NOT ACPICA engine."""
import re
from osl_818_verify import verify as verify818
H=r'0x([0-9a-f]{16})'
LINE=re.compile(r'^STYLEOS S819 TABLES registered='+H+r' maps='+H+
                r' denied-maps='+H+r' denied-hardware='+H+r' ssdts='+H+'$')
POLICY=('STYLEOS S819 POLICY acpica-linked=no acpi-mapping=checked-copy-only '
        'operationregion=deny-all ecam=not-read dma-grants=no production-cr3=deny')
def verify(raw,rc,mode):
    good,data=verify818(raw,rc,mode)
    if not good or not isinstance(raw,bytes) or raw.count(b'STYLEOS S819 ')!=3:
        return False,{}
    try:lines=raw.decode('ascii').replace('\r','').splitlines()
    except UnicodeDecodeError:return False,{}
    entries=[x for x in lines if x.startswith('STYLEOS S819 ')]
    if len(entries)!=3 or entries[1:]!=[POLICY,'STYLEOS S819 PASS']:
        return False,{}
    m=LINE.fullmatch(entries[0]);
    if not m:return False,{}
    registered,mapped,denied_maps,denied_hw,ssdt=[int(x,16) for x in m.groups()]
    if not (1<=registered<=16 and registered==ssdt+1 and
            mapped==registered and denied_maps==1 and denied_hw==6 and
            ssdt==data.get('ssdt_count_copied')):
        return False,{}
    linepos=lines.index(entries[2]);
    try:platform=next(i for i,x in enumerate(lines) if x.startswith('STYLEOS S816 PLATFORM'))
    except StopIteration:return False,{}
    if linepos>=platform or lines.index(entries[0])<=lines.index('STYLEOS S818 PASS'):
        return False,{}
    data.update({'osl819_registered':registered,'osl819_map_count':mapped,
        'osl819_denied_maps':denied_maps,'osl819_denied_hw':denied_hw,
        'osl819_acpica_linked':False,'osl819_dma_access_authorized':False})
    return True,data
