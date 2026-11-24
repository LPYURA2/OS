"""Replay S824/S825 and exact S826 diagnostic UART; no ownership grants."""
import re
from acpi_resources_824_verify import verify as parent
from test_acpi_windows_825 import reconcile
from drac_scope_826 import scope
from uefi_map_826 import audit
H=r'0x([0-9a-f]{16})'
MAP=re.compile(r'^STYLEOS S826 FINAL-MAP size='+H+r' stride='+H+r' version='+H+r' data=([0-9a-f]+)$')
DRAC=re.compile(r'^STYLEOS S826 DRAC path=\\_SB\.DRAC hid=PNP0C01 crs=([0-9a-f]{56})$')
POLICY='STYLEOS S826 POLICY observation-only=yes ecam=not-read dma-grants=no production-cr3=deny'

def need(ok,why):
    if not ok: raise ValueError(why)

def verify(raw,rc,mode):
    ok,d=parent(raw,rc,mode)
    if not ok:return False,{}
    try:
        windows=reconcile(d)
        lines=raw.decode('ascii').replace('\r','').splitlines()
        rows=[x for x in lines if x.startswith('STYLEOS S826 ')]
        need(len(rows)==3 and rows[2]==POLICY,'missing/extra S826 records')
        m=MAP.fullmatch(rows[0]);dr=DRAC.fullmatch(rows[1]);need(m and dr,'invalid S826 record')
        size,stride,version=[int(x,16) for x in m.groups()[:3]]
        need(0<size<=32768 and len(m[4])==size*2 and stride%8==0,'invalid capture length or stride')
        need(lines.index('STYLEOS S821 PASS')<lines.index(rows[0])<lines.index(rows[1])<lines.index(rows[2])<lines.index('STYLEOS S823 PASS'),'bad ordering')
        blob=bytes.fromhex(m[4]);crs=bytes.fromhex(dr[1])
        static=scope(d['_original_dsdt_bytes'])
        need(crs.hex()==static['resource_hex'] and static['interval']==windows['ecam_interval'],'DRAC differs from DSDT or MCFG')
        r=audit(blob,stride,version,*windows['ecam_interval'])
        need(r['status'] in ('GAP','EXPLICIT_NON_RAM_COVERAGE'),'ECAM conflicts with final map')
        # A gap is observed, not proven safe. Explicit coverage is evidence only.
        d.update(s826_drac_native_evaluated=True,s826_final_map_capture=True,s826_map_audit=r,
                 s826_ecam_reservation_verified=False,s826_dma_authorized=False,
                 s826_production_cr3_authorized=False,_s826_raw={'drac-crs.bin':crs,'final-map.bin':blob})
        return True,d
    except (ValueError,TypeError,IndexError,KeyError,StopIteration):return False,{}
