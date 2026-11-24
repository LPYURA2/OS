"""Check actual built PE/map and reject corrupted instruction or metadata."""
import argparse,re
from pathlib import Path
from desktop_switch_851_site import bp_site

def main():
    p=argparse.ArgumentParser();p.add_argument('--efi',type=Path,required=True);p.add_argument('--map',type=Path,required=True);a=p.parse_args()
    e=a.efi.read_bytes();m=a.map.read_text();site=bp_site(e,m);row=next(x for x in m.splitlines() if 's851_bp_return' in x.split());bad=bytearray(e);bad[site['int3_offset']]=0x90
    vals=[(bytes(bad),m),(e[:64],m),(e,m.replace(row,'')),(e,m+'\n'+row+'\n'),(e,m.replace('s851_bp_return','s851_other')),(e,m.replace(row,row.replace('boot_x86_64_desktop_preflight_850.o','other.o'))),(e,re.sub(r'Preferred load address is [0-9a-fA-F]+','Preferred load address is 0000000140001000',m)),(e,m+'\0'),(e,m.replace(row,row.replace(row.split()[0],'0002:00000001')))]
    for n,(binary,link) in enumerate(vals):
        try:bp_site(binary,link)
        except ValueError:continue
        raise AssertionError(f'invalid site accepted: {n}')
    print(f'S851 PE/map site: {site}; {len(vals)}/{len(vals)} refusals PASS')
if __name__=='__main__':main()
