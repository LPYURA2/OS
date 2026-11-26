#!/usr/bin/env python3
"""Post-EBS owned-root Block Service diagnostic; disposable Q35 fixture."""
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
    require(n==4096*(1<<21),"fixed 4TiB profile")
    run("python3", ROOT/"tools/build.py", "--arch", "x86_64",
        "--desktop-owned-shell-865", "--lba48-block-19", "--output", folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="lba48-block-19" and
            "-DSTYLEOS_LBA48_BLOCK_19=1" in manifest["flags"] and
            "kernel/ata_runtime_48.c" in manifest["sources"],"profile proof")
    return folder/"x86_64/StyleOS-owned-shell-865.iso"
def boot(folder, iso, raw, n, serial, receipt, expected_writes, fw):
    folder.mkdir()
    uart = folder/"debug.log"; nvram = folder/"OVMF_VARS.fd"
    shutil.copyfile(fw[1], nvram)
    cmd = ["qemu-system-x86_64", "-machine", "q35", "-accel", "tcg,thread=single",
           "-smp", "1", "-m", "128M", "-display", "none", "-vga", "std", "-net", "none",
           "-serial", f"file:{uart}", "-monitor", "none", "-qmp", "stdio",
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
                if uart.exists() and any(x.startswith(b"SFS19 ") and x.endswith(b"\n") for x in uart.read_bytes().splitlines(keepends=True)):
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
            rows = [x for x in uart.read_bytes().splitlines() if x.startswith(b"SFS19 ")]
            require(rows==[receipt.encode()], "unexpected guest result "+repr(rows))
            require(b"STYLEOS S863S DMA-AFTER " in uart.read_bytes() and b"STYLEOS S863S BAR4 " in uart.read_bytes(), "missing owned-root DMA admission receipts")
            require(not any(b" FAIL " in x for x in uart.read_bytes().splitlines()),"kernel failure receipt")
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
    for gib in (4096,):
        n=gib*(1<<21);folder=out/str(gib);folder.mkdir()
        iso=build(folder/"build",n);raw=folder/"disk.raw";fixture(raw,n)
        for name,receipt,writes in (("write","SFS19 WRITE PASS",1024),("cold","SFS19 COLD PASS",0)):
            results.append(boot(folder/name,iso,raw,n,SERIAL,receipt,writes,fw))
            require(raw.stat().st_size==n*512,"disk size changed")
            require(read_sector(raw,0)==TAG and read_sector(raw,1)==SENTINEL
                    and read_sector(raw,BOUNDARY-1)==SENTINEL, "protected sectors changed")
            require(read_sector(raw,BOUNDARY)==marker(1) and read_sector(raw,n-1)==marker(2),"host readback")
        if gib==4096:
            for name,actual,serial,tag,receipt in (
                    ("foreign-serial",n,"FOREIGN-SFS18",TAG,"SFS19 REFUSE identity"),
                    ("foreign-tag",n,SERIAL,b"X"*512,"SFS19 REFUSE tag"),
                    ("wrong-capacity",n//2,SERIAL,TAG,"SFS19 REFUSE identity")):
                other=folder/(name+".raw");fixture(other,actual,tag)
                results.append(boot(folder/name,iso,other,actual,serial,receipt,0,fw))
                require(read_sector(other,0)==tag and read_sector(other,1)==SENTINEL
                        and read_sector(other,BOUNDARY-1)==SENTINEL
                        and read_sector(other,BOUNDARY)==bytes(512)
                        and read_sector(other,actual-1)==bytes(512),"foreign bytes changed")
                other.unlink()
        raw.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 19 OWNED ROOT BLOCK LBA48 PASS: 2 positive, 3 refusal boots")
if __name__=="__main__":main()
