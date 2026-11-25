"""Independent extension: validate every real RTC row before legacy pump replay.
The original UART bytes are preserved and hashed; only the explicitly checked
extension rows are excluded from the older profile's exact grammar."""
import re
from runtime_pump_863_verify import site_check as old_site,verify as old_verify,trace_check,frames_check,refusals as old_refusals
from fault_site import _unpack,_range
def site_check(efi,mp):
    site=old_site(efi,mp);s=mp.read_text()
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    for name in ('rtc_runtime_bind_863','rtc_runtime_read_863'):
        rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+name+r'\s+([0-9a-fA-F]{16})\s+kernel_rtc_runtime_863\.o\s*$',s,re.M)
        if len(rows)!=1:raise ValueError('RTC linked '+name)
        seg,rel,va=(int(x,16) for x in rows[0]);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('RTC section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('RTC rights')
        _range(efi,ro+rel,1,'RTC code');site[name+'_rva']=rva
    return site
def split(raw):
    lines=raw.splitlines();idx=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863R')]
    if len(idx)!=3:raise ValueError('RTC count')
    events=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863 EVENT ')]
    ready=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863 READY ')]
    end=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S863 EXIT ')]
    wait=[i for i,x in enumerate(lines) if x==b'STYLEOS S863P WAIT']
    resume=[i for i,x in enumerate(lines) if x==b'STYLEOS S863P RESUME']
    if len(events)!=9 or any(len(x)!=1 for x in (ready,end,wait,resume)):raise ValueError('RTC shell evidence')
    if not(idx[0]+1==ready[0] and wait[0]<events[5]<idx[1]<resume[0] and events[8]<idx[2]+0<end[0]):raise ValueError('RTC phase')
    keys=('root','n','dispatches','hour','minute','second','shadow')
    result=[]
    for i,pos in enumerate(idx):
        m=re.fullmatch(b'STYLEOS S863R READ '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[pos])
        if not m:raise ValueError('RTC grammar')
        d=dict(zip(keys,map(lambda x:int(x,16),m.groups())))
        if d['n']!=i+1 or d['dispatches']!=(0,1,20)[i] or d['shadow']!=0x8d or d['hour']!=12 or not 34<=d['minute']<=36 or not 0<=d['second']<60:raise ValueError('RTC value')
        result.append(d)
    secs=[d['minute']*60+d['second'] for d in result]
    if secs!=sorted(secs):raise ValueError('RTC time went backwards')
    return b'\n'.join(x for i,x in enumerate(lines) if i not in idx)+b'\n',result
def verify(raw,rc,site,ref,frame_fnv):
    try:
        if any(not site.get(n+'_rva') for n in ('rtc_runtime_bind_863','rtc_runtime_read_863')):raise ValueError('RTC unlinked')
        base,rtc=split(raw);ok,obs=old_verify(base,rc,site,ref,frame_fnv)
        if not ok:raise ValueError(obs)
        if any(d['root']!=obs['shell_ready']['root'] for d in rtc):raise ValueError('RTC root')
        obs['rtc_runtime']=rtc;return True,obs
    except (ValueError,TypeError,KeyError,IndexError) as e:return False,str(e)
def refusals(raw,rc,site,ref,frame_fnv):
    base,_=split(raw);n=old_refusals(base,rc,site,ref,frame_fnv)
    cases=[]
    for line in raw.splitlines():
        if not line.startswith(b'STYLEOS S863R '):continue
        cases.extend([raw.replace(line,b'',1),raw.replace(line,line+b'\n'+line,1)])
        for m in re.finditer(rb'=0x([0-9a-f]{16})',line):
            a,z=m.span(1);v=int(m.group(1),16)
            cases.append(raw.replace(line,line[:a]+f'{v^(1<<63):016x}'.encode()+line[z:],1))
    for bad in cases:assert not verify(bad,rc,site,ref,frame_fnv)[0]
    for k in ('rtc_runtime_bind_863_rva','rtc_runtime_read_863_rva'):
        bad=dict(site);bad.pop(k);assert not verify(raw,rc,bad,ref,frame_fnv)[0];n+=1
    return n+len(cases)
