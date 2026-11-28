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
    package=out/"Native-peer.tea";meta=out/"manifest.json"
    with zipfile.ZipFile(package) as z:meta.write_bytes(z.read("manifest.json"))
    files="stylefs_paged_object64 stylefs_paged_stream64 stylefs_paged_pair64 stylefs_inode_root64 stylefs_inode_pages64 stylefs_namespace64 stylefs_dircommit64 stylefs_directory64 stylefs_alloc64 stylefs_map64 stylefs_extent64 block_service native_tea_stylefs native_tea_package native_tea_elf".split()
    for cc in ("gcc","clang"):
        exe=out/cc
        subprocess.run([cc,"-std=c11","-O1","-g","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-DSTYLEOS_TEA106_TEST",
          "-fsanitize=address,undefined","-fno-pie","-no-pie","-I",str(ROOT/"include"),
          str(ROOT/"tests/test_native_tea_stylefs.c"),*[str(ROOT/"kernel"/(f+".c")) for f in files],"-o",str(exe)],check=True)
        result=subprocess.check_output([exe,package,meta],text=True)
        print(cc,result,end="");(out/(cc+".log")).write_text(result)
if __name__=="__main__":main()
