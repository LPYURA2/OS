"""S821 actual upstream ACPICA utility code in guest; AML engine NOT RUN."""
import re
from acpica_bridge_819_verify import verify as verify819
H=r'0x([0-9a-f]{16})'
CORE=re.compile(r'^STYLEOS S821 ACPICA-CHECKSUM length='+H+r' known-good=yes known-bad-rejected=yes firmware-copy=yes$')
POLICY=('STYLEOS S821 POLICY upstream-real-code=yes full-core-compiled=yes '
        'aml-executed=no operationregion=deny-all dma-grants=no production-cr3=deny')

def verify(raw,rc,mode):
    if not isinstance(raw,bytes) or len(raw)>1_100_000:
        return False,{}
    ok,detail=verify819(raw,rc,mode)
    if not ok:return False,{}
    try: lines=raw.decode('ascii').replace('\r','').splitlines()
    except UnicodeDecodeError:return False,{}
    records=[s for s in lines if s.startswith('STYLEOS S821 ')]
    if len(records)!=3 or records[1:]!=[POLICY,'STYLEOS S821 PASS']:
        return False,{}
    m=CORE.fullmatch(records[0]);
    if not m:return False,{}
    length=int(m[1],16)
    if length<36 or length>24576 or length!=detail.get('dsdt_bytes'):
        return False,{}
    if any('S821 FAIL' in s for s in lines):return False,{}
    if not (lines.index('STYLEOS x86_64 ExitBootServices OK') < lines.index(records[0]) <
            lines.index(records[1]) < lines.index(records[2]) <
            lines.index('STYLEOS S818 PASS') < lines.index('STYLEOS S819 PASS')):
        return False,{}
    detail.update({'acpica821_core_checksum_real_guest':True,
                   'acpica821_guest_aml_evaluated':False,
                   'acpica821_dma_authorized':False})
    return True,detail
