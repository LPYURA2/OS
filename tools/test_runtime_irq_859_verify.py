"""Real S858 baseline + synthetic IRQ observation, or actual S859 replay."""
import json,sys
from pathlib import Path
from runtime_irq_859_verify import verify,wait_site,KEYS,POLICY,ARENA,LVT_KEYS,LVT_TAGS
from runtime_arena_858_verify import verify as prior
p=Path(sys.argv[1]);raw=(p/'serial.log').read_bytes();site=json.loads((p/'site.json').read_text());ref=json.loads((p/'reference.json').read_text());scope='actual S859 capture'
if b'STYLEOS S859 IRQ ' not in raw:
    b=Path(sys.argv[2]);site.update(wait_site((b/'BOOTX64.EFI').read_bytes(),(b/'kernel.map').read_text()))
    ok,obs=prior(raw,33,site['rva'],ref);assert ok,obs
    im=int(next(l for l in raw.splitlines() if l.startswith(b'STYLEOS MEMMAP IMAGE ')).split()[-2],16);lo=im+site['wait_lo_rva'];hi=im+site['wait_hi_rva'];rsp=obs['bp']['rsp'];v=(obs['active']['root'],obs['active']['idt'],0x400000003000,obs['bp']['frame'],obs['bp']['cs'],lo+1,lo,hi,rsp,rsp,0x202,1,1,1,2,0x10000,0,0x10f,0,1,0xfee00000,0xfee00000,0)
    row=b'STYLEOS S859 IRQ '+b' '.join(f'{k}=0x{x:016x}'.encode() for k,x in zip(KEYS,v))
    before=(v[0],0x50014,0x30020,0x10000,0x10000,0x700,0x400,0x10000);masked=before[:5]+(0x10700,0x10400)+before[7:]
    lvt=[b'STYLEOS S859 '+tag+b' '+b' '.join(f'{k}=0x{x:016x}'.encode() for k,x in zip(LVT_KEYS,values)) for tag,values in zip(LVT_TAGS,(before,masked,before))]
    raw=raw.replace(ARENA,ARENA+b'\r\n'+b'\r\n'.join(lvt+[row,POLICY]));scope='actual S858 + synthetic IRQ/LVT rows (NOT real IRQ)' 
else:row=next(l for l in raw.splitlines() if l.startswith(b'STYLEOS S859 IRQ '))
ok,obs=verify(raw,33,site,ref);assert ok,obs
n=0
for k in KEYS:
    token=next(x for x in row.split() if x.startswith(k.encode()+b'='));val=int(token.split(b'=')[1],16)
    if k=='rip':values=[0,obs['irq']['wait-hi'],obs['irq']['wait-lo']-1]
    elif k=='flags':values=[0,2,0x200,0x40202]
    else:values=[val^(1<<i) for i in range(64)]
    for value in values:
        changed=row.replace(b' '+token,b' '+k.encode()+b'='+f'0x{value:016x}'.encode(),1)
        assert not verify(raw.replace(row,changed,1),33,site,ref)[0],(k,value);n+=1
for bad in (raw.replace(row,b''),raw.replace(row,row+b'\n'+row),raw.replace(POLICY,b''),raw.replace(POLICY,POLICY.replace(b'scheduler-switch=no',b'scheduler-switch=yes')),raw.replace(row,b'').replace(ARENA,row+b'\n'+ARENA)):
    assert not verify(bad,33,site,ref)[0];n+=1
lvt=[next(l for l in raw.splitlines() if l.startswith(b'STYLEOS S859 '+tag+b' ')) for tag in LVT_TAGS]
for line in lvt:
    for changed in (b'',line+b'\n'+line):
        assert not verify(raw.replace(line,changed,1),33,site,ref)[0];n+=1
    for token in line.split()[3:]:
        k,v=token.split(b'=');val=int(v,16)
        for bit in range(64):
            changed=line.replace(b' '+token,b' '+k+b'='+f'0x{val^(1<<bit):016x}'.encode(),1)
            assert not verify(raw.replace(line,changed,1),33,site,ref)[0],(k,bit);n+=1
# Consistent but invalid bank changes must still fail.
for key in ('version','timer','thermal','perf','error'):
    for bit in range(64):
        changed=raw
        for line in lvt:
            token=next(t for t in line.split() if t.startswith(key.encode()+b'='))
            val=int(token.split(b'=')[1],16)
            changed=changed.replace(line,line.replace(token,key.encode()+b'='+f'0x{val^(1<<bit):016x}'.encode()),1)
        assert not verify(changed,33,site,ref)[0],(key,bit);n+=1
for rc in (0,35,None):assert not verify(raw,rc,site,ref)[0];n+=1
print(scope,': 1 positive /',n,'refusals PASS')
