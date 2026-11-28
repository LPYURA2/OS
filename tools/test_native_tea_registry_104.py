#!/usr/bin/env python3
import argparse
import subprocess
import zipfile
from pathlib import Path
from build_native_sample_69 import build
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument("--output",type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build(out)
    args=[]
    for name in ("Native-sample","Native-peer"):
        package=out/(name+".tea");meta=out/(name+".json")
        with zipfile.ZipFile(package) as z:meta.write_bytes(z.read("manifest.json"))
        args.extend([package,meta,out/(name+".elf")])
    for cc in ("gcc","clang"):
        exe=out/cc
        subprocess.run([cc,"-std=c11","-O1","-g","-Wall","-Wextra","-Werror",
          "-fsanitize=address,undefined","-fno-pie","-no-pie","-I",str(ROOT/"include"),
          str(ROOT/"tests/test_native_tea_registry_104.c"),str(ROOT/"kernel/native_tea_package.c"),
          str(ROOT/"kernel/native_tea_elf.c"),"-o",str(exe)],check=True)
        result=subprocess.check_output([exe,*args],text=True)
        print(cc,result,end="");(out/(cc+".log")).write_text(result)
if __name__=="__main__":main()
