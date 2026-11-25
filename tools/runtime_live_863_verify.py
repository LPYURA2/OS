"""Real in-shell bounded execution: strict phase order and independent frame evidence."""
import hashlib,re,struct
from runtime_recycle_862_verify import verify as prior,trace_check
from test_desktop_interactive_863 import site_check as prior_site, POLICY, EVENT, fnv_text, screenshots
from test_desktop_v0 import Frame
from fault_site import _unpack,_range

def site_check(efi,mp):
    site=prior_site(efi,mp);rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{16})\s+(\S+\.o)\s*$',mp.read_text(),re.M)
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF')
    symbols={}
    for name in ('s863_live_run','s863_shell_observe'):
        found=[r for r in rows if r[2]==name and r[4]=='boot_x86_64_desktop_preflight_850.o']
        if len(found)!=1:raise ValueError('S863L linked '+name)
        seg,rel,_,va,_=found[0];seg,rel,va=int(seg,16),int(rel,16),int(va,16);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S863L section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S863L code rights')
        symbols[name]=(rva,ro+rel,seg,min(vs,rs)-rel)
    start,off,seg,limit=symbols['s863_shell_observe'];after=[int(r[3],16)-site['preferred_base'] for r in rows if int(r[0],16)==seg and int(r[3],16)-site['preferred_base']>start]
    n=min([limit,*[v-start for v in after]]);code=_range(efi,off,n,'shell observer')
    calls=[start+i for i in range(len(code)-4) if code[i]==0xe8 and start+i+5+struct.unpack_from('<i',code,i+1)[0]==symbols['s863_live_run'][0]]
    if len(calls)!=1:raise ValueError('S863L observer call')
    site['s863_live_run_rva']=symbols['s863_live_run'][0];site['s863_live_call_rva']=calls[0];return site

def frames_check(out):
    hashes=screenshots(out);a=Frame(out/'text.ppm');b=Frame(out/'after-runtime.ppm')
    if (a.w,a.h)!=(b.w,b.h) or a.pixels!=b.pixels:raise ValueError('S863L frame changed during native execution')
    p=a.pixels;raw=bytearray(a.w*a.h*4);raw[0::4]=p[2::3];raw[1::4]=p[1::3];raw[2::4]=p[0::3]
    hashes['after-runtime']=hashlib.sha256((out/'after-runtime.ppm').read_bytes()).hexdigest();return hashes,fnv_text(raw)

def verify(raw,rc,site,ref,frame_fnv,*,pump=False):
    try:
        lines=raw.splitlines()
        def indices(tag):return [i for i,x in enumerate(lines) if x.startswith(tag)]
        def single(tag):
            x=indices(tag)
            if len(x)!=1:raise ValueError('S863L count '+str(tag))
            return x[0]
        def row(i,tag,keys):
            m=re.fullmatch(tag+b' '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),lines[i])
            if not m:raise ValueError('S863L schema '+str(tag))
            return dict(zip(keys,(int(x,16) for x in m.groups())))
        if rc!=33 or not site.get('s863_live_run_rva') or not site.get('s863_live_call_rva') or any(b' FAIL ' in x for x in lines):raise ValueError('S863L execution')
        a=single(b'STYLEOS S863 ADMIT ');ready=single(b'STYLEOS S863 READY ');begin=single(b'STYLEOS S863L BEGIN ');done=single(b'STYLEOS S863L DONE ');end=single(b'STYLEOS S863 EXIT ');ei=indices(b'STYLEOS S863 EVENT ')
        if pump:
            if len(ei)!=9 or not a<ready<ei[0]<ei[1]<ei[2]<ei[3]<ei[4]<begin<ei[5]<ei[6]<done<ei[7]<ei[8]<end==len(lines)-1 or begin!=ei[4]+1:raise ValueError('S863P phase order')
        elif len(ei)!=7 or not a<ready<ei[0]<ei[1]<ei[2]<ei[3]<ei[4]<begin<done<ei[5]<ei[6]<end==len(lines)-1 or begin!=ei[4]+1:raise ValueError('S863L phase order')
        if len(indices(b'STYLEOS S863L '))!=2 or len(indices(b'STYLEOS S863 '))!=(15 if pump else 13) or lines[a+1]!=POLICY:raise ValueError('S863L record count')
        # No rewriting of CPU or runtime records: only exclude shell-specific phases.
        if any(x.startswith((b'STYLEOS S862R ',b'STYLEOS S861 ',b'STYLEOS S848 ',b'STYLEOS S860 ')) for x in lines[a:begin]+lines[done+1:]):raise ValueError('S863L native execution outside request')
        native=lines[begin+1:done]
        if pump:
            native=[x for i,x in enumerate(native,begin+1) if not x.startswith(b'STYLEOS S863P ') and i not in (ei[5],ei[6])]
        elif indices(b'STYLEOS S863P '):raise ValueError('S863L unexpected pump')
        ok,obs=prior(b'\n'.join(lines[:a]+native)+b'\n',rc,site,ref,clean_fnv=frame_fnv)
        if not ok:raise ValueError(obs)
        r=obs['rounds'][1]['join_roots']['safe']
        if tuple(row(a,b'STYLEOS S863 ADMIT',('root','lpc','dispatches')).values())!=(r,0x29188086,0):raise ValueError('S863L admit')
        rd=row(ready,b'STYLEOS S863 READY',('root','dispatches','next-pid','next-window','input','x','y','writes'))
        if tuple(rd.values())!=(r,0,1,1,3,640,600,1280*800):raise ValueError('S863L ready')
        markers=(b'STYLEOS S863 STORAGE DENIED',b'STYLEOS DESKTOP DATA UNAVAILABLE',b'STYLEOS S863 CLOCK DENIED',b'STYLEOS DESKTOP KEYBOARD PASS',b'STYLEOS DESKTOP MOUSE PASS',b'STYLEOS DESKTOP READY')
        ix=[single(x) for x in markers]
        if not a+1<ix[0] or ix!=sorted(ix) or ix[-1]>=ready:raise ValueError('S863L startup')
        events=[row(i,b'STYLEOS S863 EVENT',EVENT) for i in ei]
        expected_events=[(0,0x40,0),(1,0,20),(0,0x3f,0),(0,0x1e,0),(0,0x43,0)]
        if pump:expected_events.extend(((1,0,20),(1,0,(1<<64)-20)))
        expected_events.extend(((0,0x44,0),(0,0x58,0)))
        restart=7 if pump else 5
        for i,(e,expected) in enumerate(zip(events,expected_events)):
            if (e['root'],e['n'],e['batch'],e['mouse'],e['key'],e['dx'],e['dy'],e['buttons'],e['modifiers'],e['x'],e['y'])!=(r,i+1,1,*expected,0,0,0,640 if i==0 else 680 if pump and i==5 else 660,600):raise ValueError('S863L input binding')
            if (e['ru'],e['restarts'],e['text'],e['text-hash'],e['text-dirty'],e['note-session'])!=(1,int(i>=restart),2 if i>=3 else 0,fnv_text('ф'.encode() if i>=3 else b''),int(i>=3),3 if i>=2 else 0) or e['active'] not in range(5) or e['writes']>1280*800:raise ValueError('S863L shell state')
        if not(events[0]['writes']>0 and 0<events[1]['writes']<=340 and all(events[i]['active']==4 for i in (2,3,4)) and events[2]['writes']>0 and events[3]['writes']>0 and events[4]['writes']==0 and events[restart]['writes']==1280*800 and events[restart+1]['writes']==0):raise ValueError('S863L redraw')
        for marker,lo,hi in [(b'STYLEOS DESKTOP INPUT RU',ready,ei[0]),(b'STYLEOS DESKTOP CURSOR DAMAGE PASS',ei[0],ei[1]),(b'STYLEOS DESKTOP SHELL RESTART',done,ei[restart])]:
            if not lo<single(marker)<hi:raise ValueError('S863L shell marker')
        x=row(begin,b'STYLEOS S863L BEGIN',('root','dispatches','frame','sessions'));y=row(done,b'STYLEOS S863L DONE',('root','dispatches','frame','sessions'))
        if not x['sessions'] or tuple(x.values())!=(r,0,frame_fnv,x['sessions']) or tuple(y.values())!=(r,20,frame_fnv,x['sessions']):raise ValueError('S863L retained state')
        if tuple(row(end,b'STYLEOS S863 EXIT',('root','events','dispatches','used')).values())!=(r,len(ei),20,0):raise ValueError('S863L exit')
        if pump:
            slices=indices(b'STYLEOS S863P SLICE ');backs=indices(b'STYLEOS S848 BACK ');ends=indices(b'STYLEOS S862R DONE ')
            w=single(b'STYLEOS S863P WAIT');resume=single(b'STYLEOS S863P RESUME')
            if len(slices)!=22 or len(backs)!=20 or len(ends)!=2 or len(indices(b'STYLEOS S863P '))!=24 or not begin<backs[0]<slices[0]<w<ei[5]<ei[6]<resume<backs[1]<slices[1] or lines[w]!=b'STYLEOS S863P WAIT' or lines[resume]!=b'STYLEOS S863P RESUME':raise ValueError('S863P actual input during ready task')
            if not all(e['active']==4 and 0<e['writes']<=340 for e in events[5:7]):raise ValueError('S863P pointer damage')
            proof=[]
            # Each real return is immediately followed by one shell pump record;
            # each round cleanup is a separate poll with zero dispatches.
            for n,pos in enumerate(slices):
                epoch,step=divmod(n,11);base=10*epoch
                if step==10:
                    expected=(n+1,base+10,base+10,0,0,0,int(epoch==1));prev=ends[epoch]
                else:
                    ev=obs['rounds'][epoch]['runtime_ticks'][step]
                    expected=(n+1,base+step,base+step+1,6,1 if step==0 else 2,1 if step in (0,7,8) else 0 if step==9 else 2,0);prev=backs[base+step]
                    # The exact saved runtime event, not merely an incremented poll counter.
                    if ev['dispatch']!=base+step+1:raise ValueError('S863P dispatch binding')
                z=row(pos,b'STYLEOS S863P SLICE',('n','before','after','used','processes','windows','done'))
                if tuple(z.values())!=expected or pos!=prev+1 or not begin<pos<done:raise ValueError('S863P slice boundary')
                proof.append(z)
            if slices[-1]+1!=done:raise ValueError('S863P final return')
            obs['shell_slices']=proof
        obs.update(shell_ready=rd,shell_events=events,live_begin=x,live_done=y);return True,obs
    except (ValueError,KeyError,TypeError,IndexError,StopIteration) as e:return False,str(e)

def refusals(raw,rc,site,ref,frame_fnv,*,pump=False):
    cases=[(raw,0,frame_fnv),(raw,rc,frame_fnv^1),(raw.replace(POLICY,b''),rc,frame_fnv)]
    for line in raw.splitlines():
        if not line.startswith((b'STYLEOS S863 ADMIT ',b'STYLEOS S863 READY ',b'STYLEOS S863 EVENT ',b'STYLEOS S863 EXIT ',b'STYLEOS S863L ',b'STYLEOS S863P ')):continue
        for m in re.finditer(rb'=0x([0-9a-f]{16})',line):
            a,z=m.span(1);v=int(line[a:z],16);cases.append((raw.replace(line,line[:a]+f'{v^(1<<63):016x}'.encode()+line[z:],1),rc,frame_fnv))
        cases.extend(((raw.replace(line,b'',1),rc,frame_fnv),(raw.replace(line,line+b'\n'+line,1),rc,frame_fnv)))
    for i,(bad,code,f) in enumerate(cases):
        if verify(bad,code,site,ref,f,pump=pump)[0]:raise ValueError('S863L mutation accepted '+str(i))
    return len(cases)
