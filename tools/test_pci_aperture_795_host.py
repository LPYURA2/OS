#!/usr/bin/env python3
"""Pure mask decoder test under ASAN/UBSAN and Windows EFI COFF compilation."""
import argparse,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);out=a.parse_args().output
    out.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang');assert cc,'clang required'
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    p=subprocess.run([cc,*flags,'-fsanitize=address,undefined',
        str(ROOT/'kernel/pci_aperture_795.c'),str(ROOT/'tests/test_pci_aperture_795.c'),
        '-o',str(out/'test795')],capture_output=True,text=True)
    (out/'compile.log').write_text(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('host compile failed')
    p=subprocess.run([str(out/'test795')],capture_output=True,text=True)
    (out/'host.log').write_text(p.stdout+p.stderr)
    assert p.returncode==0 and 'STYLEOS S795 BAR SIZE HOST PASS' in p.stdout,p.stderr
    p=subprocess.run([cc,'--target=x86_64-pc-win32-coff','-O2','-ffreestanding',
        '-fno-builtin','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),
        '-c',str(ROOT/'kernel/pci_aperture_795.c'),'-o',str(out/'pci795.obj')],
        capture_output=True,text=True)
    (out/'coff.log').write_text(p.stdout+p.stderr)
    assert p.returncode==0,p.stderr
    (out/'results.json').write_text(json.dumps({'stage':795,'status':'PASS',
        'sanitizers':'ASAN+UBSAN','coff':'PASS','production_cr3_authorized':False},indent=2)+'\n')
    print('STYLEOS S795 HOST PASS')
if __name__=='__main__':main()
