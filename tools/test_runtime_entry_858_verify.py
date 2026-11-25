"""Real prior frame + synthetic runtime row (or real 858 capture replay)."""
import json,sys
from pathlib import Path
from runtime_entry_858_verify import verify,KEYS,POLICY,SWITCH,FRAME
from desktop_frame_852_verify import verify as prior
p=Path(sys.argv[1]);raw=(p/'serial.log').read_bytes();rva=json.loads((p/'site.json').read_text())['rva'];ref=json.loads((p/'reference.json').read_text())
ok,obs=prior(raw,33,rva,ref);assert ok,obs
if b'STYLEOS S858 RUNTIME ' not in raw:
    a=obs['active'];b=obs['bridge'];v=(a['root'],b['old'],a['gdt'],a['idt'],a['tr'],b['cs'],obs['bp']['rsp'],0x80010001,32,0xc00,2,a['tr']+15,4095,a['root'],0,0,1,0,0)
    row=b'STYLEOS S858 RUNTIME '+b' '.join(f'{k}=0x{x:016x}'.encode() for k,x in zip(KEYS,v))
    raw=raw.replace(SWITCH,SWITCH+b'\r\n'+row).replace(FRAME,FRAME+b'\r\n'+POLICY)
    scope='synthetic runtime row over actual S852 capture'
else:
    row=next(x for x in raw.splitlines() if x.startswith(b'STYLEOS S858 RUNTIME '));scope='actual S858 replay'
ok,obs=verify(raw,33,rva,ref);assert ok,obs
n=0
for k in KEYS:
    if k in ('rsp','cr0','cr4','efer','flags'):continue
    token=next(t for t in row.split() if t.startswith(k.encode()+b'='))
    val=int(token.split(b'=')[1],16)
    for bit in range(64):
        bad=token.split(b'=')[0]+b'='+f'0x{val^(1<<bit):016x}'.encode()
        # mutate only this row/token, not safe-root or earlier stage rows
        mutated=row.replace(b' '+token,b' '+bad,1)
        assert not verify(raw.replace(row,mutated,1),33,rva,ref)[0],k;n+=1
for k,val in [('rsp',0),('cr0',0),('cr4',0),('cr4',32|128),('efer',0),('flags',514)]:
    token=next(t for t in row.split() if t.startswith(k.encode()+b'='));new=k.encode()+b'='+f'0x{val:016x}'.encode();assert not verify(raw.replace(row,row.replace(b' '+token,b' '+new),1),33,rva,ref)[0];n+=1
for bad in (raw.replace(row,b''),raw.replace(row,row+b'\n'+row),raw.replace(POLICY,b''),raw.replace(POLICY,POLICY.replace(b'irq=no',b'irq=yes')),raw.replace(POLICY,POLICY.replace(b'production-cr3=deny',b'production-cr3=yes')),raw.replace(row,b'').replace(FRAME,FRAME+b'\n'+row)):
    assert not verify(bad,33,rva,ref)[0];n+=1
for rc in (None,0,35):assert not verify(raw,rc,rva,ref)[0];n+=1
print(scope+f': 1 positive / {n} refusals PASS')
