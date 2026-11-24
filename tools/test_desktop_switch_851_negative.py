"""Parser fixture only; no QEMU claims."""
import argparse
from pathlib import Path
from desktop_switch_851_site import bp_site
from desktop_switch_851_verify import verify,POLICY
from test_desktop_preflight_850_negative import fixture,row,mutate
bp_rva=0x2000
raw=fixture()+b'\n'.join([row('S851 BRIDGE',idt=0x104b000,old=0x7801000,new=0x1060000,cs=0x38,selector=0x48),row('S851 ACTIVE',root=0x1060000,gdt=0x1044000,idt=0x1045000,tr=0x48,pat=0x0007040600070406,pge=0),row('S851 BP',root=0x1060000,frame=0x400000003000+4096-16-40,rip=0x1000000+bp_rva,rsp=0x13eff00,cs=0x38,flags=2),row('S851 PIXEL',id=0,root=0x1060000,address=0x80000000,old=0,write=0xffffff,read=0xffffff,restored=0),row('S851 PIXEL',id=1,root=0x1060000,address=0x80000000+4*(800*1280-1),old=0x12345678,write=0x12cba987,read=0x12cba987,restored=0x12345678),POLICY])+b'\n'
def main():
    p=argparse.ArgumentParser();p.add_argument('--serial',type=Path);p.add_argument('--efi',type=Path);p.add_argument('--map',type=Path);a=p.parse_args()
    capture=raw;rv=bp_rva
    if a.serial:
        if not a.efi or not a.map: p.error('--serial requires --efi and --map')
        capture=a.serial.read_bytes();rv=bp_site(a.efi.read_bytes(),a.map.read_text())['rva']
    elif a.efi or a.map: p.error('--efi/--map require --serial')
    ok,v=verify(capture,33,rv);assert ok,v
    b,x,bp,px=(v[k] for k in ('bridge','active','bp','pixels'))
    cases=[(capture,0,rv),(capture,None,rv),(capture,33,rv+1),(capture.replace(POLICY,b''),33,rv),(capture+POLICY+b'\n',33,rv),(capture+b'STYLEOS S851 FAIL bad\n',33,rv),(capture.replace(b'firmware-return=no',b'firmware-return=yes'),33,rv)]
    for tag,k,val in [('S851 BRIDGE','idt',x['gdt']),('S851 BRIDGE','old',b['new']),('S851 BRIDGE','cs',3),('S851 BRIDGE','selector',b['cs']),('S851 ACTIVE','root',b['old']),('S851 ACTIVE','gdt',x['idt']),('S851 ACTIVE','tr',x['tr']+8),('S851 ACTIVE','pge',1),('S851 ACTIVE','pat',x['pat']^1),('S851 BP','frame',bp['frame']+8),('S851 BP','rip',bp['rip']+1),('S851 BP','rsp',0),('S851 BP','flags',bp['flags']|0x200),('S851 BP','cs',bp['cs']|3),('S851 PIXEL','address',px[0]['address']+4),('S851 PIXEL','read',px[0]['read']^1),('S851 PIXEL','restored',px[0]['old']^1),('S851 PIXEL','write',px[0]['write']^1),('S851 PIXEL','id',1)]:cases.append((mutate(capture,tag,k,val),33,rv))
    for i,(bad,rc,rva) in enumerate(cases):assert not verify(bad,rc,rva)[0],i
    print(f'S851 parser: {len(cases)}/{len(cases)} refusals PASS ({"actual capture mutations" if a.serial else "synthetic only"})')
if __name__=='__main__':main()
