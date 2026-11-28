#!/usr/bin/env python3
"""Explicit native package install, source deletion, cold VM launch."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from test import ROOT, firmware_path
from test_stylefs_paged_guest_34 import boot, fixture, require, run, read_sector, N, CAP, START, BOUNDARY, SENTINEL
TAG=b"STYLEOS-SFS50-PRIVATE-V2".ljust(512,b"\0")
SERIAL="STYLEOS-SFS50-DATA"
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
    volume=out/"provision.raw";run(tool,"new50",volume)
    folder=out/"build"
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--live-volume-50","--output",folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="live-volume-50" and "-DSTYLEOS_LIVE_VOLUME_50=1" in manifest["flags"] and "-DSTYLEOS_LIVE_VOLUME_44=1" in manifest["flags"] and "-DSTYLEOS_LIVE_VOLUME_39=1" in manifest["flags"] and
        "kernel/stylefs_paged_volume64.c" in manifest["sources"],"profile proof")
    iso=folder/"x86_64/StyleOS-owned-shell-865.iso"
    package=folder/"x86_64/Native-peer.tea"
    volume.unlink();run(tool,"newtea107",volume,package)
    raw=out/"installed.raw";fixture(raw,N,volume,TAG)
    before=hashlib.sha256(volume.read_bytes()).digest()
    results=[boot(out/"install",iso,raw,N,SERIAL,"SFS39 MOUNT PASS",0,fw,live=True,install107=True,native_file88=True)]
    after=hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest()
    require(before!=after,"install did not change disk")
    results.append(boot(out/"cold-launch",iso,raw,N,SERIAL,"SFS39 MOUNT PASS",0,fw,live=True,tea69=True,tea_stop_only=True,installed107=True,native_file88=True))
    require(after==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest(),"cold installed launch changed volume")
    require(read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL,"protected sectors changed")
    raw.unlink();volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("TEA107 GUEST PASS: explicit cancel and install, source deleted, cold installed CPL3 launch and retirement, zero cold writes")
if __name__=="__main__":main()
