#!/usr/bin/env python3
"""ASAN/UBSAN strict pure BAR decoder and bridge-window checks."""
import argparse,json,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
HARNESS=r"""#include <styleos/pci_range_799.h>
#include <assert.h>
#include <stdint.h>
int main(void){
 struct pci799_extent a={0},b={0};
 struct pci794_bar x={.index=1,.memory=true,.width=32,.base=UINT64_C(0x81000000),.raw_lo=UINT32_C(0x81000000)};
 assert(pci799_decode(&x,UINT32_C(0xfffff000),0,&a));
 assert(a.base==UINT64_C(0x81000000)&&a.size==4096&&a.end==UINT64_C(0x81001000));
 assert(pci799_forwarded(&a,UINT64_C(0x81000000),UINT64_C(0x81200000)));
 assert(!pci799_forwarded(&a,UINT64_C(0x81001000),UINT64_C(0x81200000)));
 assert(!pci799_forwarded(&a,UINT64_C(0x81000000),UINT64_C(0x81000800)));
 assert(!pci799_decode(&x,UINT32_C(0xfffff800),0,&a));
 x.base++; assert(!pci799_decode(&x,UINT32_C(0xfffff000),0,&a));x.base--;
 x.raw_lo|=1u;assert(!pci799_decode(&x,UINT32_C(0xfffff001),0,&a));x.raw_lo--;
 struct pci794_bar y={.index=4,.memory=true,.width=64,.prefetch=true,
 .base=UINT64_C(0xc000000000),.raw_lo=12,.raw_hi=0xc0};
 assert(pci799_decode(&y,UINT32_C(0xffffc00c),UINT32_MAX,&b));
 assert(b.size==16384&&b.end==UINT64_C(0xc000004000));
 assert(pci799_forwarded(&b,UINT64_C(0xc000000000),UINT64_C(0xc020000000)));
 assert(!pci799_forwarded(&b,UINT64_C(0xc000001000),UINT64_C(0xc020000000)));
 assert(!pci799_decode(&y,UINT32_C(0xffffc00c),0,&b));
 y.index=5;assert(!pci799_decode(&y,UINT32_C(0xffffc00c),UINT32_MAX,&b));
 assert(!pci799_decode(0,0,0,&b));assert(!pci799_decode(&y,0,0,0));
 return 0;
}"""
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 a.output.mkdir(parents=True,exist_ok=True)
 code=a.output/'harness.c';code.write_text(HARNESS)
 gcc=shutil.which('gcc') or shutil.which('clang');assert gcc,'compiler required'
 cmd=[gcc,'-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
   '-fno-omit-frame-pointer','-I',str(ROOT/'include'),str(code),str(ROOT/'kernel/pci_range_799.c'),'-o',str(a.output/'run')]
 with (a.output/'host.log').open('w') as log:
   subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
   subprocess.run([str(a.output/'run')],stdout=log,stderr=subprocess.STDOUT,check=True)
 a.output.joinpath('results.json').write_text(json.dumps({'stage':799,'status':'PASS','sanitizers':'ASAN UBSAN','production_cr3_authorized':False},indent=2)+'\n')
 print('STYLEOS S799 HOST PASS')
if __name__=='__main__':main()
