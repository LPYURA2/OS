#!/usr/bin/env python3
"""Real C parser under ASAN/UBSAN plus freestanding COFF compilation."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path)
    out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang');assert cc,'clang required'
    proc=subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'include'),
        str(ROOT/'kernel/pci_probe_794.c'),str(ROOT/'tests/test_pci_probe_794.c'),
        '-o',str(out/'host')],capture_output=True,text=True)
    (out/'compile.log').write_text(proc.stdout+proc.stderr)
    if proc.returncode:raise SystemExit('host compile failure: '+proc.stderr)
    run=subprocess.run([str(out/'host')],capture_output=True,text=True)
    (out/'host.log').write_text(run.stdout+run.stderr)
    if run.returncode or 'STYLEOS S794 PCI PARSER HOST PASS' not in run.stdout:
        raise SystemExit('host failed: '+run.stdout+run.stderr)
    coff=subprocess.run([cc,'--target=x86_64-pc-win32-coff','-std=c11','-O2',
        '-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'include'),'-c',str(ROOT/'kernel/pci_probe_794.c'),
        '-o',str(out/'pci794.obj')],capture_output=True,text=True)
    (out/'coff.log').write_text(coff.stdout+coff.stderr)
    if coff.returncode:raise SystemExit('COFF compilation failure: '+coff.stderr)
    (out/'results.json').write_text(json.dumps({'stage':794,'status':'PASS',
        'sanitizers':'ASAN+UBSAN','coff':'PASS','production_cr3_authorized':False},indent=2)+'\n')
    print('STYLEOS S794 HOST PASS ASAN UBSAN COFF')
if __name__=='__main__':main()
