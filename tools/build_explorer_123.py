#!/usr/bin/env python3
"""Build standalone Explorer.tea with bounded RX and RW segments."""
import argparse, hashlib, json, shutil, struct, subprocess
from pathlib import Path
from tea_package import pack, inspect
ROOT=Path(__file__).resolve().parents[1]
def run(*args):
    subprocess.run(list(map(str,args)),check=True)
def build(out):
    out.mkdir(parents=True,exist_ok=False)
    app=ROOT/"apps/explorer"
    sdk=ROOT/"sdk/native"
    flags=["gcc","-std=c11","-Os","-Wall","-Wextra","-Werror","-ffreestanding",
           "-fno-builtin","-fno-pie","-fno-pic","-fno-stack-protector","-fno-asynchronous-unwind-tables",
           "-ffunction-sections","-fdata-sections","-mno-red-zone","-mgeneral-regs-only","-I"+str(sdk/"include")]
    objects=[]
    for i,p in enumerate((app/"explorer.c",app/"memory.c",sdk/"graphics.c",sdk/"x86_64/syscall.c")):
        obj=out/("%d.o"%i);run(*flags,"-c",p,"-o",obj);objects.append(obj)
    entry=out/"entry.o";run("gcc","-c",app/"entry.S","-o",entry)
    linked=out/"linked.elf"
    run("ld","--gc-sections","-T",app/"explorer.ld","-o",linked,entry,*objects)
    for section in ("text","data"):
        run("objcopy","-O","binary","--only-section=."+section,linked,out/(section+".bin"))
    code=(out/"text.bin").read_bytes();data=(out/"data.bin").read_bytes()
    assert 0<len(code)<=32768 and 0<len(data)<=65536
    align=lambda n:(n+4095)&~4095
    data_off=align(4096+len(code))
    blob=bytearray(data_off+len(data));blob[:7]=b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHIQQQIHHHHHH",blob,16,2,62,1,0x10000000,64,0,0,64,56,2,0,0,0)
    struct.pack_into("<IIQQQQQQ",blob,64,1,5,4096,0x10000000,0,len(code),align(len(code)),4096)
    struct.pack_into("<IIQQQQQQ",blob,120,1,6,data_off,0x12000000,0,len(data),align(len(data)),4096)
    blob[4096:4096+len(code)]=code;blob[data_off:]=data
    elf=out/"Explorer.elf";elf.write_bytes(blob)
    tea=out/"Explorer.tea";manifest=pack(tea,elf,"styleos.explorer","Explorer","0.1.0","x86_64")
    assert inspect(tea)==manifest
    shutil.copyfile(ROOT/"assets/icons/variant-2/explorer.svg",out/"explorer.svg")
    (out/"manifest.json").write_text(json.dumps({"manifest":manifest,"sha256":hashlib.sha256(tea.read_bytes()).hexdigest(),
        "code_bytes":len(code),"data_bytes":len(data),"bounds":[640,400],"icon_sha256":hashlib.sha256((out/"explorer.svg").read_bytes()).hexdigest()},indent=2)+"\n")
    print("EXPLORER123 PACKAGE BUILD PASS",len(code),len(data))
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--output",required=True,type=Path);build(p.parse_args().output)
