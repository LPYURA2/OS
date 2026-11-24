"""Pure strict S854 UART record validator; never proves guest execution alone."""
import re
INPUT_POLICY=b'STYLEOS S853 POLICY polling=yes irq=no storage=no clock=no reset=no app-admission=no production-cr3=deny'
POLICY=b'STYLEOS S854 POLICY terminal=yes rtc-read=yes data-write=no shadow=software nmi-mask=fixture irq=no storage=no production-cr3=deny'

def verify_record(raw,root,base):
    if type(root) is not int or root<=0 or root&4095 or base not in (12*3600+34*60,19*3600+34*60):raise ValueError('854 fixture')
    lines=raw.splitlines(); records=[(i,x) for i,x in enumerate(lines) if x.startswith(b'STYLEOS S854 ')]
    if any(b' FAIL ' in x for x in lines) or len(records)!=2 or records[1][1]!=POLICY or lines.count(INPUT_POLICY)!=1 or not lines.index(INPUT_POLICY)<records[0][0]<records[1][0]:raise ValueError('854 record order/policy')
    keys=('root','hour','minute','second','shadow')
    m=re.fullmatch(b'STYLEOS S854 RTC '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),records[0][1])
    if not m:raise ValueError('854 schema')
    r,h,mn,s,shadow=map(lambda x:int(x,16),m.groups())
    if r!=root or h>23 or mn>59 or s>59 or shadow!=0x8d or not base<=h*3600+mn*60+s<base+180:raise ValueError('854 fixture time/root/shadow')
    return dict(zip(keys,(r,h,mn,s,shadow)))
