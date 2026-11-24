"""Synthetic tests only; actual-capture mutations run in the guest runner."""
from desktop_rtc_854_verify import verify_record,INPUT_POLICY,POLICY
ROOT=0x1000;BASE=12*3600+34*60

def line(root=ROOT,hour=12,minute=34,second=0,shadow=0x8d):
    return b'STYLEOS S854 RTC '+b' '.join(f'{k}=0x{v:016x}'.encode() for k,v in zip(('root','hour','minute','second','shadow'),(root,hour,minute,second,shadow)))
def raw(r):return b'\n'.join((INPUT_POLICY,r,POLICY))
for r,base in [(line(),BASE),(line(minute=36,second=59),BASE),(line(hour=19),19*3600+34*60)]:assert verify_record(raw(r),ROOT,base)
valid=raw(line())
cases=[(raw(line(root=0x2000)),ROOT,BASE),(raw(line(hour=24)),ROOT,BASE),(raw(line(minute=60)),ROOT,BASE),(raw(line(second=60)),ROOT,BASE),(raw(line(minute=33,second=59)),ROOT,BASE),(raw(line(minute=37)),ROOT,BASE),(raw(line(shadow=0xd)),ROOT,BASE),(valid,0,BASE),(valid,ROOT,0),(valid,ROOT,19*3600+34*60),(valid.replace(POLICY,b''),ROOT,BASE),(valid+ b'\n'+POLICY,ROOT,BASE),(valid.replace(INPUT_POLICY,b''),ROOT,BASE),(b'\n'.join((line(),INPUT_POLICY,POLICY)),ROOT,BASE),(valid.replace(b'data-write=no',b'data-write=yes'),ROOT,BASE),(valid+b'\nSTYLEOS S850 FAIL rtc',ROOT,BASE),(valid.replace(b'root=0x0000000000001000',b'root=0x1000'),ROOT,BASE)]
for i,args in enumerate(cases):
    try:verify_record(*args)
    except ValueError:continue
    raise AssertionError(f'accepted mutation {i}')
print(f'RTC verifier synthetic: 3 positive / {len(cases)} refused PASS')
