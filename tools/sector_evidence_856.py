"""Deterministic fixture and byte-exact replay, NOT guest/device admission."""
import hashlib, re, pathlib, sys
N=4096; SIZE=512
POLICY=b'STYLEOS S856 READ POLICY model=yes data-write=no dma=no'
def fixture(seed):
    if seed not in (12,19): raise ValueError('seed')
    return b''.join(hashlib.sha256(b'StyleOS-856-fixture-v1'+bytes([seed])+lba.to_bytes(4,'little')+bytes([i])).digest() for lba in range(N) for i in range(16))
def verify(raw,before,after,seed,root):
    if root<=0 or root&4095 or root>=2**64: raise ValueError('root')
    if before!=fixture(seed) or after!=before: raise ValueError('disk fixture/change')
    rows=raw.splitlines()
    if len(rows)!=N+3 or rows[-1]!=POLICY: raise ValueError('record count/policy')
    if rows[0]!=f'STYLEOS S856 IDENTIFY root={root:016x} sectors={N:08x}'.encode(): raise ValueError('identify')
    for lba,row in enumerate(rows[1:N+1]):
        m=re.fullmatch(rb'STYLEOS S856 SECTOR root=([0-9a-f]{16}) lba=([0-9a-f]{8}) bytes=([0-9a-f]{1024})',row)
        if not m or int(m[1],16)!=root or int(m[2],16)!=lba: raise ValueError('sector schema/order')
        if bytes.fromhex(m[3].decode())!=before[lba*SIZE:(lba+1)*SIZE]: raise ValueError('sector bytes')
    if rows[-2]!=f'STYLEOS S856 REFUSE root={root:016x} lba={N:08x} io=0 unchanged=1'.encode(): raise ValueError('refusal')
    return {'scope':'host-model-only','sectors':N,'bytes':len(before),'sha256':hashlib.sha256(before).hexdigest()}
if __name__=='__main__':
    pathlib.Path(sys.argv[2]).write_bytes(fixture(int(sys.argv[1])))
