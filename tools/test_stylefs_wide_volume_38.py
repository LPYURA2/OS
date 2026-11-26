#!/usr/bin/env python3
"""Actual coordinated volume on private 4TiB sparse QEMU IDE fixture.
Reuse stage 20 QMP admission, disk counters and owned-root evidence."""
import argparse
import json
import struct
import subprocess
import zlib
from pathlib import Path
from test_stylefs_wide_guest_20 import ROOT, firmware_path, require, run, boot, read_sector, SENTINEL, BOUNDARY
N=4096*(1<<21)
DIR=INO=258
META=2+DIR+INO
DATA=N-1-META
START=DATA-32
IDENTITY=0x701080862838
TAG=b"STYLEOS-SFS38-PRIVATE-V1".ljust(512,b"\0")
SERIAL="STYLEOS-SFS38-DATA"
PAYLOAD=bytes((i*37+(i//512)*91+13)&255 for i in range(1041))
def record(body):
    return (body+struct.pack("<I",zlib.crc32(body))).ljust(512,b"\0")
def fixture(path,tag=TAG,corrupt=False):
    # Independent canonical receipt encoding of empty provisioned roots.
    cp=record(b"SF64WV01"+struct.pack("<13QI",1,1,1,IDENTITY,0,2,2+DIR,META,2,DIR,INO,DATA,START,0))
    directory=record(b"SF64DCM1"+struct.pack("<3QI",1,0,DIR,0))
    inode=record(b"SF64IC01"+struct.pack("<6QI",1,0,0,INO,DATA,START,0))
    with path.open("xb") as f:
        f.truncate(N*512)
        for lba,p in ((0,tag),(1,b"X"+cp[1:] if corrupt else cp),(3,directory),(3+DIR,inode),(BOUNDARY-1,SENTINEL)):
            f.seek(lba*512);f.write(p)
def main():
    p=argparse.ArgumentParser();p.add_argument("--output",type=Path,required=True)
    out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False)
    require(subprocess.check_output(["qemu-system-x86_64","--version"],text=True).startswith("QEMU emulator version 8.2.2 "),"unvalidated QEMU")
    fw=(firmware_path("OVMF_CODE","OVMF_CODE"),firmware_path("OVMF_VARS","OVMF_VARS"))
    require(all(fw),"OVMF missing")
    build=out/"build";build.mkdir()
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--wide-volume-38","--output",build)
    iso=build/"x86_64/StyleOS-owned-shell-865.iso"
    manifest=json.loads((build/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="wide-volume-38" and "-DSTYLEOS_WIDE_VOLUME_38=1" in manifest["flags"] and "kernel/stylefs_widevolume64.c" in manifest["sources"],"profile closure")
    results=[]
    raw=out/"disk.raw";fixture(raw)
    for name,receipt,writes in (("write","SFS38 WRITE PASS",59*512),("cold","SFS38 COLD PASS",0)):
        results.append(boot(out/name,iso,raw,N,SERIAL,receipt,writes,fw,prefix=b"SFS38 ",timeout=180))
        require(raw.stat().st_size==N*512 and read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL,"protected sectors changed")
        for start in (N-32,N-29):
            require(b"".join(read_sector(raw,start+i) for i in range(3))==PAYLOAD.ljust(1536,b"\0"),"raw high-LBA content or padding mismatch")
    raw.unlink()
    for name,serial,tag,corrupt,receipt in (("foreign-serial","FOREIGN-SFS38",TAG,False,"SFS38 REFUSE identity"),
          ("foreign-tag",SERIAL,b"X"*512,False,"SFS38 REFUSE tag"),("bad-receipt",SERIAL,TAG,True,"SFS38 REFUSE volume")):
        raw=out/(name+".raw");fixture(raw,tag,corrupt)
        before=[read_sector(raw,i) for i in range(META+1)]
        results.append(boot(out/name,iso,raw,N,serial,receipt,0,fw,prefix=b"SFS38 ",timeout=180))
        require(before==[read_sector(raw,i) for i in range(META+1)] and read_sector(raw,BOUNDARY-1)==SENTINEL,"refused fixture changed")
        raw.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 38 WIDE VOLUME GUEST PASS: owned-root create, mkdir, verified copy, cold reopen, listing, high-LBA raw check, three refusals")
if __name__=="__main__":main()
