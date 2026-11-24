"""Synthetic fixture only; no claim of guest execution."""
from desktop_time_855_verify import verify_record,COUNTS,POLICY,RTC_POLICY,PIT_HZ
ROOT=0x1000;BASE=12*3600+34*60;F=2_000_000_000

def record(tag,**v):return ('STYLEOS S855 '+tag+' ').encode()+b' '.join(f'{k.replace("_","-")}=0x{n:016x}'.encode() for k,n in v.items())
rows=[RTC_POLICY];lo=0;hi=10_000_000_000;start=1000000000
for i,count in enumerate(COUNTS):
    a=start;b=a+100;d=a+F*count//PIT_HZ-100;e=d+300;mn=count-2;mx=min(count+2,65536)
    lo=max(lo,(d-b)*PIT_HZ//mx);hi=min(hi,((e-a)*PIT_HZ+mn-1)//mn)
    rows.append(record('PIT',root=ROOT,id=i,count=count,start_before=a,start_after=b,end_before=d,end_after=e,min_ticks=mn,max_ticks=mx));start=e+1000
hz=lo+(hi-lo)//2
rows.append(record('CAL',root=ROOT,min_hz=lo,max_hz=hi,estimate=hz))
a=start+F;b=a+1000;c=a+F;d=c+1000
rows.append(record('EDGE',root=ROOT,id=0,second=BASE+3,lo=a,hi=b));rows.append(record('EDGE',root=ROOT,id=1,second=BASE+4,lo=c,hi=d))
rows.append(record('MONO',root=ROOT,origin=b,counter=d,ns=(d-b)*1_000_000_000//hz));rows.append(POLICY)
raw=b'\n'.join(rows);assert verify_record(raw,ROOT,BASE)['estimate_hz']==hz
cases=[(raw,0,BASE),(raw,ROOT,BASE+180),(raw.replace(POLICY,b''),ROOT,BASE),(raw+b'\n'+POLICY,ROOT,BASE),(raw.replace(RTC_POLICY,b''),ROOT,BASE),(raw.replace(b'pit2=stopped',b'pit2=running'),ROOT,BASE),(raw+b'\nSTYLEOS S850 FAIL time',ROOT,BASE)]
for i in range(1,8):cases.append((b'\n'.join(rows[:i]+rows[i+1:]),ROOT,BASE))
for old,new in [(b'count=0x000000000000e90b',b'count=0x0000000000000001'),(rows[1],rows[1].replace(b'id=0x0000000000000000',b'id=0x0000000000000001')),(rows[4],rows[4].split(b' estimate=')[0]+b' estimate=0x0000000000000000'),(rows[6],rows[6].replace(f'second=0x{BASE+4:016x}'.encode(),f'second=0x{BASE+5:016x}'.encode())),(rows[7],rows[7].split(b' ns=')[0]+b' ns=0x0000000000000000')]:
    assert old in raw;cases.append((raw.replace(old,new),ROOT,BASE))
for i,args in enumerate(cases):
    try:verify_record(*args)
    except ValueError:continue
    raise AssertionError(f'accepted mutation {i}')
print(f'S855 verifier synthetic: 1 positive, {len(cases)} refusal PASS')
