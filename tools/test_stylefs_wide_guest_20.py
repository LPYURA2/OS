#!/usr/bin/env python3
"""Wide journal and stream through real post-EBS LBA48; disposable fixture."""
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
TAG = b"STYLEOS-SFS20-PRIVATE-V1".ljust(512, b"\0")
SERIAL = "STYLEOS-SFS20-DATA"
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
N=4096*(1<<21)
CAP=N-1
START=CAP-32
PAYLOAD=bytes((i*37+(i//512)*91+13)&255 for i in range(1041))
def fixture(p,n,volume,tag=TAG,corrupt=False):
    with p.open("xb") as f:
        f.truncate(n*512)
        f.seek(0); f.write(tag)
        f.seek(512)
        with volume.open("rb") as v:
            metadata=v.read(19*512)
        f.write(metadata if not corrupt else b"X"+metadata[1:])
        f.seek((BOUNDARY-1)*512); f.write(SENTINEL)
def export_check(tool,raw,folder):
    image=folder/"verify.raw"
    with image.open("xb") as f,raw.open("rb") as q:
        f.truncate(CAP*512)
        q.seek(512);f.write(q.read(19*512))
        q.seek((START+1)*512);f.seek(START*512);f.write(q.read(32*512))
    result=folder/"export.bin"
    run(tool,"get",image,START,1,result)
    require(result.read_bytes()==PAYLOAD,"independent host file export mismatch")
    require(subprocess.run([str(tool),"stat",str(image),str(START),"2"],capture_output=True).returncode==1,
            "cancelled file published")
    image.unlink()
def build(folder, n):
    folder.mkdir()
    require(n==4096*(1<<21),"fixed 4TiB profile")
    run("python3", ROOT/"tools/build.py", "--arch", "x86_64",
        "--desktop-owned-shell-865", "--wide-guest-20", "--output", folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="wide-guest-20" and
            "-DSTYLEOS_WIDE_GUEST_20=1" in manifest["flags"] and
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
                if uart.exists() and any(x.startswith(b"SFS20 ") and x.endswith(b"\n") for x in uart.read_bytes().splitlines(keepends=True)):
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
            rows = [x for x in uart.read_bytes().splitlines() if x.startswith(b"SFS20 ")]
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
    tool=out/"wide-image"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT",
        "-I",ROOT/"include",ROOT/"tools/stylefs_wide_image.c",
        *[ROOT/("kernel/"+s+".c") for s in ("stylefs_stream64","stylefs_journal64","stylefs_alloc64","stylefs_map64","stylefs_extent64","block_service")],"-o",tool)
    volume=out/"provision.raw";run(tool,"new",volume,CAP,START)
    iso=build(out/"build",N)
    raw=out/"disk.raw";fixture(raw,N,volume)
    initial_super=read_sector(raw,1)
    for name,receipt,writes in (("write","SFS20 WRITE PASS",13*512),("cold","SFS20 COLD PASS",0)):
        results.append(boot(out/name,iso,raw,N,SERIAL,receipt,writes,fw))
        require(raw.stat().st_size==N*512,"disk size changed")
        require(read_sector(raw,0)==TAG and read_sector(raw,1)==initial_super and
                read_sector(raw,BOUNDARY-1)==SENTINEL,"protected sectors changed")
        require(read_sector(raw,START+1)==PAYLOAD[:512] and
                read_sector(raw,START+2)==PAYLOAD[512:1024] and
                read_sector(raw,START+3)==PAYLOAD[1024:].ljust(512,b"\0"),"high-sector payload/tail")
        export_check(tool,raw,out/name)
    raw.unlink()
    for name,n,serial,tag,corrupt,receipt in (
            ("foreign-serial",N,"FOREIGN-SFS20",TAG,False,"SFS20 REFUSE identity"),
            ("foreign-tag",N,SERIAL,b"X"*512,False,"SFS20 REFUSE tag"),
            ("wrong-capacity",N//2,SERIAL,TAG,False,"SFS20 REFUSE identity"),
            ("bad-journal",N,SERIAL,TAG,True,"SFS20 REFUSE journal")):
        raw=out/(name+".raw");fixture(raw,n,volume,tag,corrupt)
        before=[read_sector(raw,x) for x in range(20)]
        results.append(boot(out/name,iso,raw,n,serial,receipt,0,fw))
        require(before==[read_sector(raw,x) for x in range(20)] and
                read_sector(raw,BOUNDARY-1)==SENTINEL,"refused disk changed")
        raw.unlink()
    volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 20 WIDE GUEST PASS: mount, staged cancellation, high-LBA file, cold reopen, host export, four refusals")
if __name__=="__main__":main()
