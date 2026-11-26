"""Verify full real framebuffer readback after S851 transition."""
import re
from desktop_switch_851_verify import verify as prior, POLICY as SWITCH, CLEAN_POLICY
POLICY=b'STYLEOS S852 POLICY first-frame=yes input=no storage=no app-admission=no production-cr3=deny'

def verify(raw,rc,rva,reference,*,host_quit=False,clean_boot=False):
    try:
        ok,obs=prior(raw,rc,rva,host_quit=host_quit,clean_boot=clean_boot)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines();records=[x for x in lines if x.startswith(b'STYLEOS S852 ')]
        if len(records)!=2 or records[1]!=POLICY or lines.index(CLEAN_POLICY if clean_boot else SWITCH)>=lines.index(records[0]) or lines.index(records[0])>=lines.index(records[1]):raise ValueError('852 record order/policy')
        if any(b' FAIL ' in x for x in lines):raise ValueError('guest failure')
        keys=('root','width','height','writes','fnv','first','last')
        match=re.fullmatch(b'STYLEOS S852 FRAME '+b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),records[0])
        if not match:raise ValueError('852 schema')
        v=dict(zip(keys,(int(x,16) for x in match.groups())))
        g=obs['gop'];r=obs['active']['root']
        if (g['width'],g['height'],g['stride'],g['format'],g['bytes'])!=(1280,800,1280,1,4096000):raise ValueError('852 mode')
        if (v['root'],v['width'],v['height'],v['writes'])!=(r,1280,800,1024000):raise ValueError('852 frame contract')
        if not isinstance(reference,dict) or set(reference)!=set(('writes','fnv','first','last')) or any(type(x)!=int for x in reference.values()):raise ValueError('852 host reference')
        if any(v[k]!=reference[k] for k in reference) or any(v[k]>0xffffffff for k in ('first','last')):raise ValueError('852 framebuffer mismatch')
        obs['desktop_frame']=v
        return True,obs
    except (ValueError,IndexError,TypeError,KeyError) as e:return False,str(e)
