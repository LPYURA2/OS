"""Byte-exact ATA fixture transcript; no generic disk admission."""
import re,struct
from functools import lru_cache
from busmaster_857_verify import verify_transition
from sector_evidence_856 import fixture
POLICY=b'STYLEOS S857 ATA-POLICY fixture-only=yes identify=yes read=yes data-write=no dma=no production-cr3=deny'
LBAS=(0,1,2048,4095)
@lru_cache(maxsize=1)
def expected_fixture():return fixture(19)
def verify_read(raw,root,topology,base):
    if any(b' FAIL ' in x for x in raw.splitlines()):raise ValueError('guest failure')
    rows=[x for x in raw.splitlines() if x.startswith(b'STYLEOS S857 ')]
    if len(rows)!=12 or rows[-1]!=POLICY:raise ValueError('ATA record count/policy')
    bm=verify_transition(b'\n'.join(rows[:4]),root,topology)
    if base!=expected_fixture():raise ValueError('fixture')
    prefix=f' root=0x{root:016x}'.encode()
    def data(row,tag,fields=b''):
        m=re.fullmatch(b'STYLEOS S857 '+tag+prefix+fields+b' bytes=([0-9a-f]{1024})',row)
        if not m:raise ValueError('ATA payload/schema')
        return bytes.fromhex(m[1].decode())
    identify=data(rows[4],b'ATA-IDENTIFY');w=struct.unpack('<256H',identify)
    if not w[0] or w[0]==65535 or w[0]&0x8000 or not w[49]&0x200 or w[106]&0xd000==0x5000 or (w[60]|w[61]<<16)!=4096:raise ValueError('ATA geometry')
    for row,lba in zip(rows[5:9],LBAS):
        if data(row,b'ATA-SECTOR',f' lba=0x{lba:016x}'.encode())!=base[lba*512:(lba+1)*512]:raise ValueError('sector mismatch')
    if rows[9]!=b'STYLEOS S857 ATA-REFUSE'+prefix+b' lba=0x0000000000001000 io=0x0000000000000000 unchanged=0x0000000000000001' or rows[10]!=b'STYLEOS S857 ATA-READY'+prefix:raise ValueError('refusal/ready')
    return {'fixture_only':True,'geometry_sectors':4096,'sector_reads':list(LBAS),'bytes_compared':2048,'oob_refused':True,'generic_admission':False}
