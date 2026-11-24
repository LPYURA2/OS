"""S855 independent integer replay of captured PIT and RTC brackets."""
import re
from desktop_rtc_854_verify import POLICY as RTC_POLICY
POLICY=b'STYLEOS S855 POLICY terminal=yes pit2=stopped reference=fixture rtc-crosscheck=yes irq=no storage=no production-cr3=deny'
COUNTS=(59659,47727,65535)
PIT_HZ=1193182
MAX_HZ=10_000_000_000

def verify_record(raw,root,base):
    lines=raw.splitlines(); records=[(i,s) for i,s in enumerate(lines) if s.startswith(b'STYLEOS S855 ')]
    if not root or root&4095 or len(records)!=8 or lines.count(RTC_POLICY)!=1 or not lines.index(RTC_POLICY)<records[0][0] or records[-1][1]!=POLICY or any(b' FAIL ' in x for x in lines):raise ValueError('855 records/policy')
    def parse(i,tag,keys):
        m=re.fullmatch(('STYLEOS S855 '+tag+' ').encode()+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),records[i][1]);
        if not m:raise ValueError('855 schema '+tag)
        values=tuple(int(v,16) for v in m.groups())
        if values[0]!=root:raise ValueError('855 root')
        return values[1:]
    low,high=0,MAX_HZ;last=0;windows=[]
    for i,count in enumerate(COUNTS):
        n,c,a,b,d,e,mn,mx=parse(i,'PIT',('root','id','count','start-before','start-after','end-before','end-after','min-ticks','max-ticks'))
        if (n,c,mn,mx)!=(i,count,count-2,min(count+2,65536)) or (i and last>a) or not 0<a<=b<d<=e:raise ValueError('855 PIT window')
        lo=(d-b)*PIT_HZ//mx;hi=((e-a)*PIT_HZ+mn-1)//mn
        if not 0<lo<=hi<=MAX_HZ or hi-lo>lo//100:raise ValueError('855 PIT width')
        low=max(low,lo);high=min(high,hi);last=e;windows.append([a,b,d,e,mn,mx])
    if not 0<low<=high or high-low>low//100:raise ValueError('855 intersection')
    hz=low+(high-low)//2
    if parse(3,'CAL',('root','min-hz','max-hz','estimate'))!=(low,high,hz):raise ValueError('855 estimate')
    edges=[]
    for i in range(2):
        n,sec,a,b=parse(4+i,'EDGE',('root','id','second','lo','hi'))
        if n!=i or not base<=sec<base+180 or not last<=a<=b:raise ValueError('855 RTC edge')
        edges.append((sec,a,b));last=b
    (s0,l0,h0),(s1,l1,h1)=edges
    if s1!=s0+1 or l1<=h0 or l1-h0<hz-hz//20 or h1-l0>hz+hz//20:raise ValueError('855 RTC crosscheck')
    origin,counter,ns=parse(6,'MONO',('root','origin','counter','ns'))
    if (origin,counter)!=(h0,h1) or ns!=(h1-h0)*1_000_000_000//hz or not ns:raise ValueError('855 monotonic')
    return {'windows':windows,'min_hz':low,'max_hz':high,'estimate_hz':hz,'edges':edges,'ns':ns}
