"""S852 strict parser tests; synthetic or real capture mutations only."""
import argparse,json
from pathlib import Path
from desktop_frame_852_verify import verify,POLICY
from desktop_switch_851_site import bp_site
from test_desktop_switch_851_negative import raw,bp_rva
from test_desktop_preflight_850_negative import row,mutate
REF={'writes':1024000,'fnv':14631338882325684282,'first':1647410,'last':1054754}
def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path);a=p.parse_args()
    if a.capture:
        q=a.capture;capture=(q/'serial.log').read_bytes();ref=json.loads((q/'reference.json').read_text());rva=bp_site((q/'build/x86_64/BOOTX64.EFI').read_bytes(),(q/'build/x86_64/kernel.map').read_text())['rva']
    else:
        rva=bp_rva;ref=REF;capture=raw+row('S852 FRAME',root=0x1060000,width=1280,height=800,**ref)+b'\n'+POLICY+b'\n'
    ok,obs=verify(capture,33,rva,ref);assert ok,obs
    v=obs['desktop_frame'];cases=[(capture,0,rva,ref),(capture,None,rva,ref),(capture,33,rva+1,ref),(capture,33,rva,{}),(capture,33,rva,{**ref,'fnv':ref['fnv']^1}),(capture.replace(POLICY,b''),33,rva,ref),(capture+POLICY+b'\n',33,rva,ref),(capture+b'STYLEOS S852 FAIL denied\n',33,rva,ref),(capture.replace(b'input=no',b'input=yes'),33,rva,ref),(capture.replace(b'storage=no',b'storage=yes'),33,rva,ref)]
    for k in v:cases.append((mutate(capture,'S852 FRAME',k,v[k]^1),33,rva,ref))
    for i,bad in enumerate(cases):assert not verify(*bad)[0],i
    print(f'S852 {len(cases)}/{len(cases)} parser refusals PASS ({"actual capture mutations" if a.capture else "synthetic only"})')
if __name__=='__main__':main()
