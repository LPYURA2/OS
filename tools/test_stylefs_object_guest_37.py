#!/usr/bin/env python3
"""Separate object-broker fixture, reuse admitted QMP verifier from stage 34."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from test import ROOT, firmware_path
from test_stylefs_paged_guest_34 import boot, fixture, require, run, read_sector, N, CAP, START, BOUNDARY, SENTINEL, PAYLOAD
TAG=b"STYLEOS-SFS37-PRIVATE-V1".ljust(512,b"\0")
SERIAL="STYLEOS-SFS37-DATA"
def main():
    p=argparse.ArgumentParser();p.add_argument("--output",type=Path,required=True)
    out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False)
    require(subprocess.check_output(["qemu-system-x86_64","--version"],text=True).startswith("QEMU emulator version 8.2.2 "), "unvalidated QEMU")
    fw=(firmware_path("OVMF_CODE","OVMF_CODE"),firmware_path("OVMF_VARS","OVMF_VARS"))
    require(all(fw),"OVMF missing")
    tool=out/"fixture"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-I",ROOT/"include",
        ROOT/"tools/stylefs_paged_fixture_34.c",
        *[ROOT/("kernel/"+s+".c") for s in ("stylefs_paged_stream64","stylefs_paged_pair64","stylefs_inode_root64","stylefs_inode_pages64","stylefs_namespace64","stylefs_dircommit64","stylefs_directory64","stylefs_alloc64","stylefs_map64","stylefs_extent64","block_service")],"-o",tool)
    volume=out/"provision.raw";run(tool,"new",volume)
    folder=out/"build"
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--object-guest-37","--output",folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="object-guest-37" and "-DSTYLEOS_OBJECT_GUEST_37=1" in manifest["flags"] and
        "kernel/stylefs_paged_object64.c" in manifest["sources"],"profile proof")
    iso=folder/"x86_64/StyleOS-owned-shell-865.iso"
    raw=out/"disk.raw";fixture(raw,N,volume,TAG)
    results=[]
    for name,receipt,writes in (("write","SFS34 OBJECT WRITE PASS",60*512),("cold","SFS34 OBJECT COLD PASS",0)):
        results.append(boot(out/name,iso,raw,N,SERIAL,receipt,writes,fw))
        require(raw.stat().st_size==N*512 and read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL,"protected disk changed")
        image=out/name/"verify.raw"
        with raw.open("rb") as q:
            q.seek(START*512);image.write_bytes(q.read(CAP*512))
        exported=out/name/"export.bin";run(tool,"get37",image,exported)
        require(exported.read_bytes()==bytes(x^255 for x in PAYLOAD),"host export mismatch")
        image.unlink()
    raw.unlink()
    for name,serial,tag,receipt in (("foreign-serial","FOREIGN-SFS37",TAG,"SFS34 REFUSE identity"),
            ("foreign-tag",SERIAL,b"X"*512,"SFS34 REFUSE tag")):
        raw=out/(name+".raw");fixture(raw,N,volume,tag)
        before=hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest()
        results.append(boot(out/name,iso,raw,N,serial,receipt,0,fw))
        require(before==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest(),"refused disk changed")
        raw.unlink()
    volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 37 OBJECT GUEST PASS: scoped read/write, wrong principal, cancellation, stale token, cold reopen, independent export")
if __name__=="__main__":main()
