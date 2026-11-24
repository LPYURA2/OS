import argparse,re
from pathlib import Path
from timer_scheduler_843_verify import verify
p=argparse.ArgumentParser();p.add_argument('--input',required=True,type=Path);a=p.parse_args()
raw=a.input.read_bytes();assert verify(raw,33)[0]
lines=raw.splitlines();i=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S843 TICK '))
cases=[]
for field in (b'n',b'from',b'to',b'root',b'frame',b'rip',b'rsp',b'a',b'b',b'gprs',b'eoi',b'r15',b'rax'):
    bad=lines[:];bad[i]=re.sub(rb'(?<![a-z-])'+field+rb'=0x([0-9a-f]{16})',lambda m:field+b'=0x'+f'{int(m[1],16)^0x100000000000:016x}'.encode(),bad[i],count=1)
    assert bad[i]!=lines[i];cases.append(b'\n'.join(bad)+b'\n')
cases.extend([b'\n'.join(lines[:i]+lines[i+1:])+b'\n',b'\n'.join(lines[:i]+[lines[i]]+lines[i:])+b'\n',raw.replace(b'cpl3=no production-cr3=deny',b'cpl3=yes production-cr3=allow')])
for bad in cases:assert not verify(bad,33)[0]
assert not verify(raw,0)[0]
print(f'S843 genuine-capture host refusals: {len(cases)+1} PASS')
# Synthetic replay of a real capture: exercise pending IRQ accounting without
# claiming that the inserted records were emitted by the guest.
base=[x for x in lines if not x.startswith(b'STYLEOS S843 DEFER ')]
di=next(j for j,x in enumerate(base) if x.startswith(b'STYLEOS S807 DELIVERY '))
base[di]=re.sub(rb'hits=0x[0-9a-f]{16}',b'hits=0x0000000000000010',base[di])
i=next(j for j,x in enumerate(base) if x.startswith(b'STYLEOS S843 TICK '))
assert verify(b'\n'.join(base)+b'\n',33)[0]
def replay(n):
    tick=base[i]
    first=tick.replace(b'S843 TICK ',b'S843 DEFER ',1)
    first=re.sub(rb' to=0x[0-9a-f]{16}',b'',first)
    first=re.sub(rb'(?<![a-z-])[ab]=0x[0-9a-f]{16}',lambda m:m[0][:2]+b'0x0000000000000000',first)
    result=base[:]
    result[di]=re.sub(rb'hits=0x[0-9a-f]{16}',f'hits=0x{16+n:016x}'.encode(),result[di])
    ds=[first.replace(b' from=',f' count=0x{k+1:016x} from='.encode(),1) for k in range(n)]
    return result[:i]+ds+result[i:]
def valid(xs):return verify(b'\n'.join(xs)+b'\n',33)[0]
assert valid(replay(1)) and valid(replay(16))
assert not valid(replay(17))
mutations=0
for key in (b'n',b'count',b'from',b'root',b'frame',b'rip',b'rsp',b'a',b'b',b'gprs',b'eoi',b'rax'):
    xs=replay(1)
    xs[i]=re.sub(rb'(?<![a-z-])'+key+rb'=0x([0-9a-f]{16})',lambda m:key+b'=0x'+f'{int(m[1],16)^0x100000000000:016x}'.encode(),xs[i],count=1)
    assert not valid(xs),key
    mutations+=1
xs=replay(1);xs.pop(i);assert not valid(xs)
xs=replay(1);xs.insert(i,xs[i]);assert not valid(xs)
print(f'S843 synthetic deferred-IRQ replay: 2 admissible + {mutations+3} refused PASS')
