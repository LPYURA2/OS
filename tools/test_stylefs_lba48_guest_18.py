#!/usr/bin/env python3
"""Terminal EFI LBA48 diagnostic; isolated Q35 fixture, never real host disks."""
import argparse
import hashlib
import json
import shutil
import subprocess
import time
from pathlib import Path
from test import ROOT, firmware_path
from test_desktop_v0 import Desktop
from block_fixture_856_verify import CONTROLLER, DEVICE, controller_identity, tree_device
from guest_fixture_857 import verify_legacy_io
from legacy_ports_857 import verify_ready
from bmdma_857_verify import verify_bar

BOUNDARY = 1 << 28
TAG = b"STYLEOS-SFS18-PRIVATE-V1".ljust(512, b"\0")
SERIAL = "STYLEOS-SFS18-DATA"
SENTINEL = b"PROTECTED-LOW-LBA".ljust(512, b"\x9a")
def require(ok, why):
    if not ok:
        raise RuntimeError(why)
def run(*args):
    subprocess.run(list(map(str, args)), check=True)
def marker(slot):
    return bytes((i*37+slot*91+13)&255 for i in range(512))
def read_sector(p, lba):
    with p.open("rb") as f:
        f.seek(lba*512)
        return f.read(512)
def fixture(p, n, tag=TAG):
    with p.open("xb") as f:
        f.truncate(n*512)
        f.seek(0); f.write(tag)
        f.seek(512); f.write(SENTINEL)
        f.seek((BOUNDARY-1)*512); f.write(SENTINEL)
def build(folder, n):
    folder.mkdir()
    obj = []
    for source in ("tests/guest/stylefs_lba48_18.c", "kernel/ata_runtime_48.c"):
        o = folder / (Path(source).stem + ".obj")
        run("clang", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O2",
            "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-mno-red-zone",
            "-Wall", "-Wextra", "-Werror", "-I", ROOT/"include",
            "-DFIXTURE_SECTORS="+str(n)+"ULL", "-c", ROOT/source, "-o", o)
        obj.append(o)
    efi = folder/"BOOTX64.EFI"
    run("lld-link", "/subsystem:efi_application", "/entry:efi_main", "/nodefaultlib",
        "/machine:x64", "/out:"+str(efi), *obj)
    iso_dir = folder/"iso"; iso_dir.mkdir()
    fat = iso_dir/"efiboot.img"
    with fat.open("xb") as f: f.truncate(8*1024*1024)
    run("mkfs.vfat", fat)
    run("mmd", "-i", fat, "::/EFI", "::/EFI/BOOT")
    run("mcopy", "-i", fat, efi, "::/EFI/BOOT/BOOTX64.EFI")
    iso = folder/"StyleOS-lba48-diagnostic.iso"
    run("xorriso", "-as", "mkisofs", "-R", "-e", "efiboot.img", "-no-emul-boot",
        "-o", iso, iso_dir)
    (folder/"boot-sha256.txt").write_text(hashlib.sha256(iso.read_bytes()).hexdigest()+"\n")
    return iso
def boot(folder, iso, raw, n, serial, receipt, expected_writes, fw):
    folder.mkdir()
    uart = folder/"debug.log"; nvram = folder/"OVMF_VARS.fd"
    shutil.copyfile(fw[1], nvram)
    cmd = ["qemu-system-x86_64", "-machine", "q35", "-accel", "tcg,thread=single",
           "-smp", "1", "-m", "128M", "-display", "none", "-vga", "std", "-net", "none",
           "-serial", "none", "-debugcon", f"file:{uart}", "-monitor", "none", "-qmp", "stdio",
           "-no-reboot", "-S", "-drive", f"if=pflash,format=raw,readonly=on,file={fw[0]}",
           "-drive", f"if=pflash,format=raw,file={nvram}", "-boot", "order=d,strict=on",
           "-cdrom", str(iso), "-device", "piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5",
           "-blockdev", json.dumps({"driver":"file","filename":str(raw),"node-name":"s18-file"}),
           "-blockdev", json.dumps({"driver":"raw","file":"s18-file","node-name":"s18-disk"}),
           "-device", f"ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive=s18-disk,serial={serial}"]
    (folder/"command.json").write_text(json.dumps(cmd, indent=2))
    with (folder/"qemu.log").open("wb") as err:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err)
        d = Desktop(proc, uart, folder)
        try:
            require("QMP" in d.receive(), "QMP greeting")
            d.call("qmp_capabilities")
            def snapshot(label):
                o = {}
                for key, method, kw in [
                    ("status","query-status",{}), ("pci","query-pci",{}),
                    ("blocks","query-block",{}), ("nodes","query-named-block-nodes",{}),
                    ("qtree","human-monitor-command",{"command-line":"info qtree"}),
                    ("mtree","human-monitor-command",{"command-line":"info mtree -f -o"}),
                    ("stats","query-blockstats",{"query-nodes":False})]:
                    o[key] = d.call(method, **kw)
                (folder/(label+".json")).write_text(json.dumps(o, indent=2))
                require(o["status"].get("running") is False, "not paused")
                devices = [x for bus in o["pci"] if bus["bus"]==0 for x in bus["devices"]]
                ide = [x for x in devices if x.get("qdev_id")==CONTROLLER]
                require(len(ide)==1 and controller_identity(ide[0]), "wrong controller")
                tree_device(o["qtree"]); verify_legacy_io(o["mtree"])
                blocks = [x for x in o["blocks"] if x.get("qdev") in (DEVICE,"/machine/peripheral/"+DEVICE)]
                require(len(blocks)==1, "wrong attachment")
                ins = blocks[0]["inserted"]
                require(ins["node-name"]=="s18-disk" and ins["image"]["virtual-size"]==n*512
                        and ins["drv"]=="raw" and not ins["ro"], "wrong raw geometry")
                nodes = [x for x in o["nodes"] if x.get("node-name") in ("s18-file","s18-disk")]
                require(len(nodes)==2 and all(x.get("file")==str(raw) and not x.get("ro") for x in nodes),
                        "wrong backing path")
                stats = [x["stats"] for x in o["stats"] if x.get("qdev") in (DEVICE,"/machine/peripheral/"+DEVICE)]
                require(len(stats)==1, "wrong stats")
                return o,stats[0]
            _, start = snapshot("prelaunch")
            d.call("cont")
            deadline = time.monotonic()+100
            while time.monotonic()<deadline and proc.poll() is None:
                if uart.exists() and any(x.startswith(b"SFS18 ") for x in uart.read_bytes().splitlines()):
                    break
                time.sleep(.05)
            else:
                raise RuntimeError("no guest receipt")
            d.call("stop")
            o, final = snapshot("final")
            verify_ready(o)
            bar = [x for bus in o["pci"] for x in bus["devices"] if x.get("qdev_id")==CONTROLLER][0]["regions"]
            require(len(bar)==1,"BAR count")
            verify_bar(o["pci"],o["mtree"],bar[0]["address"])
            rows = [x for x in uart.read_bytes().splitlines() if x.startswith(b"SFS18 ")]
            require(rows==[receipt.encode()], "unexpected guest result "+repr(rows))
            require(final["wr_bytes"]-start["wr_bytes"]==expected_writes, "write byte count")
            require(final["unmap_bytes"]==start["unmap_bytes"], "unexpected unmap")
            if expected_writes:
                require(final["flush_operations"]-start["flush_operations"]>=2, "flush evidence")
            for k in ("failed_wr_operations","invalid_wr_operations","failed_flush_operations","invalid_flush_operations"):
                require(final.get(k,0)==start.get(k,0), k)
            d.call("quit")
            require(proc.wait(timeout=20)==0, "QEMU exit")
        finally:
            d.ready.close()
            if proc.poll() is None:
                proc.kill(); proc.wait()
    return {"receipt":receipt,"written_bytes":expected_writes,"sectors":n}
def main():
    p=argparse.ArgumentParser();p.add_argument("--output",type=Path,required=True)
    out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False)
    require(subprocess.check_output(["qemu-system-x86_64","--version"],text=True).startswith("QEMU emulator version 8.2.2 "), "unvalidated QEMU")
    fw=(firmware_path("OVMF_CODE","OVMF_CODE"),firmware_path("OVMF_VARS","OVMF_VARS"))
    require(all(fw),"OVMF missing")
    results=[]
    for gib in (256,512,1024,4096):
        n=gib*(1<<21);folder=out/str(gib);folder.mkdir()
        iso=build(folder/"build",n);raw=folder/"disk.raw";fixture(raw,n)
        for name,receipt,writes in (("write","SFS18 WRITE PASS",1024),("cold","SFS18 COLD PASS",0)):
            results.append(boot(folder/name,iso,raw,n,SERIAL,receipt,writes,fw))
            require(raw.stat().st_size==n*512,"disk size changed")
            require(read_sector(raw,0)==TAG and read_sector(raw,1)==SENTINEL
                    and read_sector(raw,BOUNDARY-1)==SENTINEL, "protected sectors changed")
            require(read_sector(raw,BOUNDARY)==marker(1) and read_sector(raw,n-1)==marker(2),"host readback")
        if gib==256:
            for name,actual,serial,tag,receipt in (
                    ("foreign-serial",n,"FOREIGN-SFS18",TAG,"SFS18 REFUSE identity"),
                    ("foreign-tag",n,SERIAL,b"X"*512,"SFS18 REFUSE tag"),
                    ("wrong-capacity",n*2,SERIAL,TAG,"SFS18 REFUSE identity")):
                other=folder/(name+".raw");fixture(other,actual,tag)
                results.append(boot(folder/name,iso,other,actual,serial,receipt,0,fw))
                require(read_sector(other,0)==tag and read_sector(other,1)==SENTINEL
                        and read_sector(other,BOUNDARY-1)==SENTINEL
                        and read_sector(other,BOUNDARY)==bytes(512)
                        and read_sector(other,actual-1)==bytes(512),"foreign bytes changed")
                other.unlink()
        raw.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 18 QEMU EFI LBA48 PASS: 8 positive, 3 refusal boots")
if __name__=="__main__":main()
