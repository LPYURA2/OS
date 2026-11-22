#!/usr/bin/env python3
import argparse,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
cc=shutil.which('clang') or shutil.which('cc')
if not cc:raise RuntimeError('Host compiler required')
subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
 '-fno-omit-frame-pointer','-I',str(ROOT/'include'),str(ROOT/'kernel/cache_pagewalk_801.c'),
 str(ROOT/'tests/test_cache_pagewalk_801.c'),'-o',str(out/'cache801_host')],check=True)
with (out/'host.log').open('w') as f:
 subprocess.run([str(out/'cache801_host')],stdout=f,stderr=subprocess.STDOUT,check=True)
subprocess.run([cc,'--target=x86_64-pc-win32-coff','-ffreestanding','-std=c11',
 '-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-c',str(ROOT/'kernel/cache_pagewalk_801.c'),
 '-o',str(out/'cache801_coff.o')],check=True)
(out/'results.json').write_text(json.dumps({'stage':801,'status':'PASS','asan_ubsan':True,
 'coff_compilation':True,'production_cr3_authorized':False,'effective_mmio_cache_verified':False},indent=2)+'\n')
print('STYLEOS 801 host ASAN UBSAN COFF PASS')
