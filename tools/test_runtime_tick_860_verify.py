import re,sys
from pathlib import Path
from runtime_tick_860_verify import verify,KEYS,POLICY
from fx_context_849_verify import verify as prior
from user_preempt_848_verify import ORDER
raw=Path(sys.argv[1]).read_bytes();scope='actual S860 capture'
if b'STYLEOS S860 READY ' not in raw:
    ok,obs=prior(raw,33);assert ok,obs;r=obs['child_roots'];safe=r['safe'];lines=raw.splitlines()
    lease=next(x for x in lines if x.startswith(b'STYLEOS S846 READY '));handles=[int(re.search(rb'\bgen-'+k+rb'=0x([0-9a-f]{16})',lease)[1],16) for k in (b'a',b'b')]
    def row(tag,keys,values):return b'STYLEOS S860 '+tag+b' '+b' '.join(k.encode()+b'='+f'0x{v:016x}'.encode() for k,v in zip(keys,values))
    ready=row(b'READY',('safe','pid-a','pid-b','handle-a','handle-b','win-a','win-b'),(safe,1,2,*handles,1,2))
    ticks=[0,0];step=0;out=[]
    for line in lines:
        if line.startswith(b'STYLEOS S848 READY '):out.append(ready)
        if line.startswith(b'STYLEOS S848 BACK '):
            id=ORDER[step];term=step in (6,9)
            if not term:ticks[id]+=1
            out.append(row(b'EVENT',KEYS,(step,id,id+1,handles[id],step+1,safe,r['ab'[id]],ticks[id],(4 if id else 5) if term else 1,12288,0 if term else id+1,0 if term else int(id==0))));step+=1
        out.append(line)
        if line.startswith(b'STYLEOS S846 POLICY '):out.extend([row(b'CLEAN',('safe','dispatches','processes','windows'),(safe,10,0,0)),POLICY])
    raw=b'\n'.join(out)+b'\n';scope='actual S849 + synthetic S860 rows (NOT guest S860)'
ok,obs=verify(raw,33);assert ok,obs
n=0
for row in [x for x in raw.splitlines() if x.startswith(b'STYLEOS S860 ')]:
    for changed in (b'',row+b'\n'+row):assert not verify(raw.replace(row,changed,1),33)[0];n+=1
    for token in row.split()[3:]:
        if b'=0x' not in token:continue
        k,v=token.split(b'=');val=int(v,16)
        for bit in range(64):
            changed=row.replace(b' '+token,b' '+k+b'='+f'0x{val^(1<<bit):016x}'.encode(),1)
            assert not verify(raw.replace(row,changed,1),33)[0],(k,bit);n+=1
for rc in (0,35,None):assert not verify(raw,rc)[0];n+=1
print(scope,': 1 positive /',n,'refusals PASS')
