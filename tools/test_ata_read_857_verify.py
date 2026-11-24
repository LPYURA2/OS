"""Actual prior capture + synthetic ATA payload. Not a real ATA success record."""
import sys,json,struct
from pathlib import Path
from ata_read_857_verify import verify_read,LBAS,POLICY
from storage_topology_856_verify import verify
from sector_evidence_856 import fixture
p=Path(sys.argv[1]);raw=(p/'serial.log').read_bytes();res=json.loads((p/'result.json').read_text());root=res['root'];t=verify(raw,root,json.loads((p/'ready-pci.json').read_text()));base=fixture(19)
w=[0]*256;w[0]=0x40;w[49]=0x200;w[60]=4096;w[106]=0x4000
prefix=f' root=0x{root:016x}'.encode()
rows=[b'STYLEOS S857 ATA-IDENTIFY'+prefix+b' bytes='+struct.pack('<256H',*w).hex().encode()]
rows += [b'STYLEOS S857 ATA-SECTOR'+prefix+f' lba=0x{lba:016x}'.encode()+b' bytes='+base[lba*512:(lba+1)*512].hex().encode() for lba in LBAS]
rows += [b'STYLEOS S857 ATA-REFUSE'+prefix+b' lba=0x0000000000001000 io=0x0000000000000000 unchanged=0x0000000000000001',b'STYLEOS S857 ATA-READY'+prefix,POLICY]
raw=raw.rstrip()+b'\n';append=lambda r:raw+b'\n'.join(r)+b'\n';assert verify_read(append(rows),root,t,base)['bytes_compared']==2048
n=0
def reject(r,b=base):
    global n
    try:verify_read(append(r),root,t,b)
    except ValueError:n+=1;return
    raise AssertionError('mutated record accepted')
for i in range(len(rows)):reject(rows[:i]+rows[i+1:]);reject(rows[:i]+[rows[i]]+rows[i:])
for i in range(1,5):
    offset=rows[i].index(b' bytes=')+7
    for byte in range(512):
        r=rows.copy();data=bytearray(r[i]);j=offset+byte*2;data[j]=ord('1') if data[j]==ord('0') else ord('0');r[i]=bytes(data);reject(r)
for index,bad in [(0,0),(0,65535),(0,0x8000),(49,0),(60,4095),(61,1),(106,0x5000)]:
    z=w.copy();z[index]=bad;r=rows.copy();r[0]=rows[0].split(b' bytes=')[0]+b' bytes='+struct.pack('<256H',*z).hex().encode();reject(r)
for i in range(7):
    r=rows.copy();r[i]=r[i].replace(prefix,b' root=0x0000000000001000');reject(r)
for old,new in [(b'data-write=no',b'data-write=yes'),(b'dma=no',b'dma=yes'),(b'fixture-only=yes',b'fixture-only=no')]:r=rows.copy();r[-1]=r[-1].replace(old,new);reject(r)
reject(rows,base[:-1]);r=rows.copy();r[1],r[2]=r[2],r[1];reject(r)
print('Actual BMIDE prior + synthetic ATA transcript: 1 positive /',n,'mutations rejected; no actual ATA execution')
