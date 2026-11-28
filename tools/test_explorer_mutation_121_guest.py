#!/usr/bin/env python3
"""Separate disposable ID-managed fixture: actual CPL3 mutations."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from test import ROOT, firmware_path
from test_stylefs_paged_guest_34 import boot, fixture, require, run, read_sector, N, START, BOUNDARY, SENTINEL
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
    folder=out/"build"
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--live-volume-50","--explorer-probe-121","--output",folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="live-volume-50" and "-DSTYLEOS_EXPLORER_PROBE_121=1" in manifest["flags"],"isolated profile")
    iso=folder/"x86_64/StyleOS-owned-shell-865.iso"
    package=folder/"x86_64/Native-peer.tea"
    volume=out/"provision.raw";run(tool,"newtea121",volume,package)
    raw=out/"edited.raw";fixture(raw,N,volume,TAG)
    before=hashlib.sha256(volume.read_bytes()).digest()
    try:
        result=boot(out/"edit",iso,raw,N,SERIAL,"SFS39 MOUNT PASS",0,fw,live=True,tea69=True,tea_stop_only=True,native_file88=True,explorer_probe121=True)
    except Exception:
        log=out/"edit/debug.log"
        if log.exists():print(log.read_bytes()[-16384:].decode("ascii",errors="replace"),flush=True)
        raise
    recovered=out/"recovered.raw"
    recovered.write_bytes(b"".join(read_sector(raw,x) for x in range(START,N)))
    require(before!=hashlib.sha256(recovered.read_bytes()).digest(),"no durable edits")
    require(read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL,"protected sectors changed")
    run(tool,"check121",recovered,"verified")
    (out/"results.json").write_text(json.dumps(result,indent=2)+"\n")
    raw.unlink();volume.unlink();recovered.unlink()
    print("EXPLORER121 GUEST PASS: real CPL3 namespace edits, durable independent reopen, retirement")
if __name__=="__main__":main()
