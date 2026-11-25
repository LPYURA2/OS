"""Synthetic arena records on actual S858 capture OR actual combined capture."""
import json,sys
from pathlib import Path
from runtime_entry_858_verify import verify as prior,POLICY as RUNTIME
from runtime_arena_858_verify import verify,POLICY,ARENA_POLICY
p=Path(sys.argv[1]);raw=(p/'serial.log').read_bytes();site=json.loads((p/'site.json').read_text());ref=json.loads((p/'reference.json').read_text())
if b'STYLEOS S858 ARENA ' not in raw:
    ok,obs=prior(raw,33,site['rva'],ref);assert ok,obs
    # Only a fabricated receipt for parser testing; the actual S858 capture
    # contains no EFI allocation. Never claim this address was granted.
    base=0x7e77000;root=obs['active']['root'];end=base+65536
    def row(tag,fields):return tag.encode()+b' '+b' '.join(f'{k}=0x{v:016x}'.encode() for k,v in fields)
    a=row('STYLEOS S844 RESERVE',[('base',base),('pages',16)])
    b=row('STYLEOS S844 ADMIT',[('base',base),('end',end),('desc-start',base),('desc-end',end),('type',2),('attr',8)])
    c=row('STYLEOS S844 LIVE',zip(('root','a','b','reuse','gen-a','gen-b','gen-reuse','used','zero','refusals'),(root,base,base+16384,base,1,2,3,0,1,4)))
    d=row('STYLEOS S858 ARENA',zip(('root','base','pages','used','next','zero'),(root,base,16,0,4,1)))
    def insert(prefix,lines):
        global raw
        x=next(x for x in raw.splitlines() if x.startswith(prefix));raw=raw.replace(x,b'\r\n'.join(lines)+b'\r\n'+x,1)
    insert(b'STYLEOS x86_64 acquiring memory map',[a]);insert(b'STYLEOS S842 READY ',[b]);insert(b'STYLEOS S858 RUNTIME ',[c,ARENA_POLICY]);raw=raw.replace(RUNTIME,RUNTIME+b'\r\n'+d+b'\r\n'+POLICY,1)
    scope='actual S858 + synthetic arena receipt/lifecycle'
else:scope='actual combined capture'
ok,obs=verify(raw,33,site['rva'],ref);assert ok,obs
rows=[x for x in raw.splitlines() if x.startswith((b'STYLEOS S844 ',b'STYLEOS S858 ARENA'))];n=0
for row in rows:
    for altered in (b'',row+b'\n'+row):
        assert not verify(raw.replace(row,altered,1),33,site['rva'],ref)[0];n+=1
    if b'POLICY' in row:continue
    for t in row.split()[3:]:
        k,v=t.split(b'=');v=int(v,16)
        # Flips in all exact lifecycle fields; descriptor bounds and attr
        # intentionally permit some valid alternatives, so test only invalid.
        values=[v^(1<<i) for i in range(64)] if k not in (b'desc-start',b'desc-end',b'attr') else [1] if k!=b'attr' else [0,1<<63]
        for value in values:
            bad=row.replace(b' '+t,b' '+k+b'='+f'0x{value:016x}'.encode(),1)
            assert not verify(raw.replace(row,bad,1),33,site['rva'],ref)[0],(k,value);n+=1
for rc in (0,35,None):assert not verify(raw,rc,site['rva'],ref)[0];n+=1
print(scope,': 1 positive /',n,'refusals PASS')
