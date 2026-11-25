"""S862 bounded fixture: validate chosen context against the existing S861 proof."""
import re
from runtime_timer_861_verify import verify as prior, timer_site, trace_check
from fault_site import _unpack,_range

def select_site(efi, map_file):
    site=timer_site(efi,map_file)
    rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+rt862_selected_context\s+([0-9a-fA-F]{16})\s+kernel_process_trap_847\.o\s*$',map_file.read_text(),re.M)
    if len(rows)!=1:raise ValueError('S862 selected context missing from linked image')
    seg,rel,va=(int(v,16) for v in rows[0]);rva=va-site['preferred_base']
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    if not 1<=seg<=count:raise ValueError('S862 section')
    _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
    if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S862 symbol range/rights')
    _range(efi,ro+rel,1,'S862 entry byte');site['selected_context_rva']=rva
    return site

def verify(raw,rc,site,reference):
    try:
        ok,obs=prior(raw,rc,site,reference)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();rows=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S862 ')]
        frames=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S861T FRAME ')]
        backs=[i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S848 BACK ')]
        ready=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S848 READY '))
        if len(rows)!=10 or len(frames)!=10 or len(backs)!=10:raise ValueError('S862 selection count')
        pat=rb'STYLEOS S862 SELECT '+b' '.join(k+rb'=0x([0-9a-f]{16})' for k in (b'step',b'id',b'pid',b'dispatch',b'cursor',b'safe',b'root'))
        evidence=[]
        for step,(i,f,b,e) in enumerate(zip(rows,frames,backs,obs['runtime_ticks'])):
            m=re.fullmatch(pat,lines[i]);want=(step,e['id'],e['pid'],e['dispatch'],0,e['safe'],e['root'])
            if not m or tuple(int(x,16) for x in m.groups())!=want or not (backs[step-1] if step else ready)<i<f<b:raise ValueError('S862 selection binding/order')
            evidence.append(dict(zip(('step','id','pid','dispatch','cursor','safe','root'),want)))
        if not any(x['id']!=x['cursor'] for x in evidence):raise ValueError('S862 cursor disagreement absent')
        obs['selected_contexts']=evidence;return True,obs
    except (ValueError,KeyError,IndexError,TypeError,StopIteration) as e:return False,str(e)
