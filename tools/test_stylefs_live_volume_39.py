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
    p.add_argument("--waiting-stop-only",action="store_true")
    p.add_argument("--file-probe-92",action="store_true")
    args=p.parse_args()
    require(not args.file_probe_92 or args.waiting_stop_only,"file probe needs isolated one-client stop")
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    require(subprocess.check_output(["qemu-system-x86_64","--version"],text=True).startswith("QEMU emulator version 8.2.2 "), "unvalidated QEMU")
    fw=(firmware_path("OVMF_CODE","OVMF_CODE"),firmware_path("OVMF_VARS","OVMF_VARS"))
    require(all(fw),"OVMF missing")
    tool=out/"fixture"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-I",ROOT/"include",
        ROOT/"tools/stylefs_paged_fixture_34.c",
        *[ROOT/("kernel/"+s+".c") for s in ("stylefs_paged_stream64","stylefs_paged_pair64","stylefs_inode_root64","stylefs_inode_pages64","stylefs_namespace64","stylefs_dircommit64","stylefs_directory64","stylefs_alloc64","stylefs_map64","stylefs_extent64","block_service")],"-o",tool)
    volume=out/"provision.raw";run(tool,"new39",volume)
    folder=out/"build"
    run("python3",ROOT/"tools/build.py","--arch","x86_64","--desktop-owned-shell-865","--live-volume-39",
        *(["--file-probe-92"] if args.file_probe_92 else []),"--output",folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="live-volume-39" and "-DSTYLEOS_LIVE_VOLUME_39=1" in manifest["flags"] and
        "kernel/stylefs_paged_volume64.c" in manifest["sources"] and
        "kernel/native_file_85.c" in manifest["sources"],"profile proof")
    require(("-DSTYLEOS_FILE_PROBE_92=1" in manifest["flags"])==args.file_probe_92,"diagnostic isolation")
    iso=folder/"x86_64/StyleOS-owned-shell-865.iso"
    results=[]
    for name,serial,tag,receipt in (() if args.waiting_stop_only else (("live",SERIAL,TAG,"SFS39 MOUNT PASS"),("foreign-tag",SERIAL,b"X"*512,"SFS39 REFUSE tag"))):
        raw=out/(name+".raw");fixture(raw,N,volume,tag)
        before=hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest()
        try:
            results.append(boot(out/name,iso,raw,N,serial,receipt,0,fw,live=True,native_file88=True))
            uart=(out/name/"debug.log").read_bytes()
            expected=1 if name=="live" else 0
            count=uart.splitlines().count(b"STYLEOS NATIVE FILE88 READY")
            require(count==expected,f"{name}: native file bind count {count}, expected {expected}")
            lines=uart.splitlines()
            require(lines.count(b"STYLEOS NATIVE FILE90 QUEUE-READY")==expected,
                    f"{name}: unexpected file queue initialization")
            state=b"STYLEOS FILE88 BOOT-STATE "+(b"ready" if expected else b"unavailable")
            desk=[i for i,x in enumerate(lines) if x.startswith(b"STYLEOS S863 READY ")]
            require(lines.count(state)==1 and bool(desk) and lines.index(state)<desk[0],
                    f"{name}: wrong file boot state or readiness order")
        except Exception:
            uart=out/name/"debug.log"
            if uart.exists():
                with uart.open("rb") as stream:
                    stream.seek(max(0,uart.stat().st_size-16384))
                    print(stream.read().decode("ascii",errors="replace"),flush=True)
            raise
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
    cases=(("tea-waiting-stop",package,True),) if args.waiting_stop_only else (("tea-valid",package,True),("tea-invalid",damaged,False))
    for name,source,valid in cases:
        volume=out/(name+"-volume.raw");run(tool,"newtea69",volume,source)
        raw=out/(name+".raw");fixture(raw,N,volume,TAG)
        before=hashlib.sha256(volume.read_bytes()).digest()
        try:
            results.append(boot(out/name,iso,raw,N,SERIAL,"SFS39 MOUNT PASS",0,fw,live=True,tea69=valid,tea_stop_only=args.waiting_stop_only,native_file88=True,file_probe92=args.file_probe_92))
        except Exception:
            # Bounded diagnostic tail in the job log even if upload fails.
            uart=out/name/"debug.log"
            if uart.exists():
                with uart.open("rb") as stream:
                    stream.seek(max(0,uart.stat().st_size-16384))
                    print(stream.read().decode("ascii",errors="replace"),flush=True)
            raise
        uart=(out/name/"debug.log").read_bytes()
        count=uart.splitlines().count(b"STYLEOS NATIVE FILE88 READY")
        require(count==1,f"{name}: native file service bind count {count}, expected 1")
        require(uart.splitlines().count(b"STYLEOS NATIVE FILE90 QUEUE-READY")==1,
                f"{name}: native file queue not initialized exactly once")
        require(read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL and before==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(START,N))).digest(),"TEA volume changed")
        raw.unlink();volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    if args.waiting_stop_only:
        print("TEA70 WAITING STOP PASS: fresh native application, confirmed UI stop, resource retirement, zero writes")
    else:
        print("STYLEFS 39 LIVE VOLUME PASS: mount, desktop ready, input-boundary read, foreign media refusal, zero writes")
if __name__=="__main__":main()
