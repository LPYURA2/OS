#!/usr/bin/env python3
"""Host-only synthetic bridge tests; cannot count as hardware attestation."""
import argparse,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path)
    out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
    cc=shutil.which('clang');assert cc,'clang required'
    args=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    run=subprocess.run([*args,'-fsanitize=address,undefined',str(ROOT/'kernel/pci_topology_796.c'),
        str(ROOT/'tests/test_pci_topology_796.c'),'-o',str(out/'test796')],capture_output=True,text=True)
    (out/'compile.log').write_text(run.stdout+run.stderr)
    if run.returncode:raise RuntimeError('host compiler: '+run.stderr)
    run=subprocess.run([str(out/'test796')],capture_output=True,text=True)
    (out/'host.log').write_text(run.stdout+run.stderr)
    if run.returncode or 'STYLEOS S796 TOPOLOGY HOST PASS' not in run.stdout:
        raise RuntimeError('host ASAN/UBSAN: '+run.stderr)
    run=subprocess.run([cc,'--target=x86_64-pc-win32-coff','-O2','-ffreestanding',
        '-fno-builtin','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),
        '-c',str(ROOT/'kernel/pci_topology_796.c'),'-o',str(out/'topology.obj')],capture_output=True,text=True)
    (out/'coff.log').write_text(run.stdout+run.stderr)
    if run.returncode:raise RuntimeError('COFF compile: '+run.stderr)
    (out/'results.json').write_text(json.dumps({'stage':796,'status':'PASS',
        'sanitizers':'ASAN+UBSAN','coff':'PASS','production_cr3_authorized':False},indent=2)+'\n')
    print('STYLEOS S796 HOST PASS')
if __name__=='__main__':main()
