#!/usr/bin/env python3
import argparse,json,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);o=p.parse_args().output;o.mkdir(parents=True,exist_ok=True)
 cc=shutil.which('clang') or 'clang'
 args=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-I',str(ROOT/'boot/x86_64')]
 src=ROOT/'kernel/hardware_probe_793.c'
 cmd=[cc,*args,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(src),str(ROOT/'tests/test_hardware_probe_793.c'),'-o',str(o/'host')]
 subprocess.run(cmd,check=True)
 r=subprocess.run([o/'host'],capture_output=True,text=True)
 (o/'test.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 subprocess.run([cc,'--target=x86_64-pc-win32-coff',*args,'-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float','-c',str(src),'-o',str(o/'probe.obj')],check=True)
 (o/'results.json').write_text(json.dumps({'stage':793,'status':'PASS','asan_ubsan':True,'coff':True,'production_cr3_authorized':False},indent=2)+'\n')
 print(r.stdout,end='');print('STYLEOS 793 HOST ASAN UBSAN COFF PASS')
if __name__=='__main__':main()
