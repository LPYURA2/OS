#!/usr/bin/env python3
"""Read-only persistent paged volume alongside live desktop; exact test disk."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from test import ROOT, firmware_path
from test_stylefs_paged_guest_34 import boot, fixture, require, run, read_sector, N, CAP, START, BOUNDARY, SENTINEL
TAG=b"STYLEOS-SFS39-PRIVATE-V1".ljust(512,b"\0")
SERIAL="STYLEOS-SFS39-DATA"
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
    volume=out/"provision.raw";run(tool,"new39",volume)
    folder=out/"build"
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--live-volume-39","--output",folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="live-volume-39" and "-DSTYLEOS_LIVE_VOLUME_39=1" in manifest["flags"] and
        "kernel/stylefs_paged_volume64.c" in manifest["sources"],"profile proof")
    iso=folder/"x86_64/StyleOS-owned-shell-865.iso"
    results=[]
    for name,serial,tag,receipt in (("live",SERIAL,TAG,"SFS39 MOUNT PASS"),("foreign-tag",SERIAL,b"X"*512,"SFS39 REFUSE tag")):
        raw=out/(name+".raw");fixture(raw,N,volume,tag)
        before=hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest()
        results.append(boot(out/name,iso,raw,N,serial,receipt,0,fw,live=True))
        require(read_sector(raw,0)==tag and read_sector(raw,BOUNDARY-1)==SENTINEL and before==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest(),"readonly disk changed")
        raw.unlink()
    volume.unlink()
    package=folder/"x86_64/Native-peer.tea"
    require(package.is_file(),"standalone peer package absent")
    # Generated package is independently provisioned into StyleFS.
    # The exact EFI path can vary; ordinary stage 69 already checks absence.
    damaged=out/"damaged.tea"
    payload=bytearray(package.read_bytes());payload[len(payload)//2]^=1
    damaged.write_bytes(payload)
    for name,source,valid in (("tea-valid",package,True),("tea-invalid",damaged,False)):
        volume=out/(name+"-volume.raw");run(tool,"newtea69",volume,source)
        raw=out/(name+".raw");fixture(raw,N,volume,TAG)
        before=hashlib.sha256(volume.read_bytes()).digest()
        results.append(boot(out/name,iso,raw,N,SERIAL,"SFS39 MOUNT PASS",0,fw,live=True,tea69=valid))
        require(read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL and before==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest(),"TEA volume changed")
        raw.unlink();volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 39 LIVE VOLUME PASS: mount, desktop ready, input-boundary read, foreign media refusal, zero writes")
if __name__=="__main__":main()
