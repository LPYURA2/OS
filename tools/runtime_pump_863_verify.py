"""Real shell polling across bounded hardware quanta. No rewritten evidence."""
import hashlib,re,struct
from runtime_live_863_verify import site_check as prior_site,verify as prior_verify,frames_check as prior_frames,refusals as prior_refusals,trace_check
from fault_site import _unpack,_range
from test_desktop_v0 import Frame

def site_check(efi,mp):
    site=prior_site(efi,mp);rows=re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{16})\s+(\S+\.o)\s*$',mp.read_text(),re.M)
    pe,=_unpack('<I',efi,60,'PE');_,count,_,_,_,opt,_=_unpack('<HHIIIHH',efi,pe+4,'COFF');syms={}
    for name,obj in [('desktop_v0_run','kernel_desktop_shell.o'),('s863_shell_pump','boot_x86_64_desktop_preflight_850.o'),('guest861','boot_x86_64_desktop_preflight_850.o'),('s848_trial','boot_x86_64_user_preemption_848.o')]:
        rr=[r for r in rows if r[2]==name and r[4]==obj]
        if len(rr)!=1:raise ValueError('S863P linked '+name)
        seg,rel,_,va,_=rr[0];seg,rel,va=int(seg,16),int(rel,16),int(va,16);rva=va-site['preferred_base']
        if not 1<=seg<=count:raise ValueError('S863P section')
        _,vs,v,rs,ro,_,_,_,_,flags=_unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if rva!=v+rel or not 0<=rel<min(vs,rs) or not flags&0x20000000 or flags&0x80000000:raise ValueError('S863P code rights')
        after=[int(r[3],16)-site['preferred_base'] for r in rows if int(r[0],16)==seg and int(r[3],16)-site['preferred_base']>rva]
        syms[name]=(rva,ro+rel,min([min(vs,rs)-rel,*[v-rva for v in after]]))
    for caller,callee in [('desktop_v0_run','s863_shell_pump'),('s863_shell_pump','guest861'),('guest861','s848_trial')]:
        start,off,n=syms[caller];code=_range(efi,off,n,'S863P caller');target=syms[callee][0]
        calls=[start+i for i in range(len(code)-4) if code[i]==0xe8 and start+i+5+struct.unpack_from('<i',code,i+1)[0]==target]
        if len(calls)!=1:raise ValueError('S863P call '+caller+' '+callee)
        site[callee+'_rva']=target;site[callee+'_call_rva']=calls[0]
    return site

def frames_check(out):
    hashes,fnv=prior_frames(out);text=Frame(out/'text.ppm');moved=Frame(out/'during-runtime.ppm')
    if (text.w,text.h)!=(moved.w,moved.h) or text.pixels==moved.pixels:raise ValueError('S863P no actual pointer redraw during task')
    text.expect_changes_in(moved,[(660,600,10,17),(680,600,10,17)])
    hashes['during-runtime']=hashlib.sha256((out/'during-runtime.ppm').read_bytes()).hexdigest();return hashes,fnv

def verify(raw,rc,site,ref,frame_fnv):
    if any(not site.get(x) for x in ('s863_shell_pump_rva','s863_shell_pump_call_rva','guest861_rva','guest861_call_rva','s848_trial_rva','s848_trial_call_rva')):return False,'S863P binary not linked'
    return prior_verify(raw,rc,site,ref,frame_fnv,pump=True)

def refusals(raw,rc,site,ref,frame_fnv):
    n=prior_refusals(raw,rc,site,ref,frame_fnv,pump=True)
    for k in ('s863_shell_pump_rva','s863_shell_pump_call_rva','guest861_rva','guest861_call_rva','s848_trial_rva','s848_trial_call_rva'):
        bad=dict(site);bad.pop(k);assert not verify(raw,rc,bad,ref,frame_fnv)[0];n+=1
    return n
