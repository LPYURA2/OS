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
TAG = b"STYLEOS-SFS34-PRIVATE-V1".ljust(512, b"\0")
SERIAL = "STYLEOS-SFS34-DATA"
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
CAP=512
START=N-CAP
PAYLOAD=bytes((i*37+(i//512)*91+13)&255 for i in range(1041))
def fixture(p,n,volume,tag=TAG,corrupt=False):
    with p.open("xb") as f:
        f.truncate(n*512)
        f.seek(0);f.write(tag)
        f.seek((n-CAP)*512)
        metadata=volume.read_bytes()
        f.write((b"X"+metadata[1:]) if corrupt else metadata)
        f.seek((BOUNDARY-1)*512);f.write(SENTINEL)
def export_check(tool,raw,folder):
    image=folder/"verify.raw"
    with raw.open("rb") as q:
        q.seek(START*512);image.write_bytes(q.read(CAP*512))
    result=folder/"export.bin"
    run(tool,"get",image,result)
    require(result.read_bytes()==PAYLOAD,"independent host export mismatch")
    image.unlink()
def build(folder, n):
    folder.mkdir()
    require(n==4096*(1<<21),"fixed 4TiB profile")
    run("python3", ROOT/"tools/build.py", "--arch", "x86_64",
        "--desktop-owned-shell-865", "--paged-guest-34", "--output", folder)
    manifest=json.loads((folder/"x86_64/owned-shell-compiler.json").read_text())
    require(manifest["profile"]=="paged-guest-34" and
            "-DSTYLEOS_PAGED_GUEST_34=1" in manifest["flags"] and
            "kernel/ata_runtime_48.c" in manifest["sources"],"profile proof")
    return folder/"x86_64/StyleOS-owned-shell-865.iso"
def boot(folder, iso, raw, n, serial, receipt, expected_writes, fw, live=False):
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
            deadline = time.monotonic()+180
            while time.monotonic()<deadline and proc.poll() is None:
                if uart.exists():
                    lines=uart.read_bytes().splitlines(keepends=True)
                    failures=[x for x in lines if b" FAIL " in x and x.endswith(b"\n")]
                    require(not failures,"guest fail-stop: "+repr(failures))
                    if live and any(x.rstrip(b"\r\n")==b"STYLEOS DESKTOP READY" and x.endswith(b"\n") for x in lines) and any(x.startswith(b"STYLEOS S863 READY ") and x.endswith(b"\n") for x in lines):
                        break
                    if not live and any(x.startswith(b"SFS34 ") and x.endswith(b"\n") for x in lines):
                        break
                time.sleep(.05)
            else:
                raise RuntimeError("no guest receipt")
            if live and receipt=="SFS39 MOUNT PASS":
                require(b"SFS39 DESKTOP READ PASS" in uart.read_bytes().splitlines(),
                        "mounted object not readable before input")
                # A single relative mouse event, not an absolute-coordinate helper.
                d.call("input-send-event", events=[{"type":"rel","data":{"axis":"x","value":20}}])
                # Desktop.wait_uart prepends STYLEOS DESKTOP; this receipt
                # belongs to the volume service, so match the full line.
                deadline=time.monotonic()+40
                while time.monotonic()<deadline and proc.poll() is None:
                    lines=uart.read_bytes().splitlines()
                    require(not any(b" FAIL " in x for x in lines),"guest failure after input")
                    if b"SFS39 INPUT READ PASS" in lines and any(x.startswith(b"STYLEOS S863 EVENT ") for x in lines):
                        break
                    time.sleep(.05)
                else:
                    raise RuntimeError("live volume input receipt missing")
                d.key("f2")
                deadline=time.monotonic()+40
                while time.monotonic()<deadline and proc.poll() is None:
                    lines=uart.read_bytes().splitlines()
                    require(not any(b" FAIL " in x for x in lines),"guest failure opening Explorer")
                    if b"SFS41 EXPLORER ROWS" in lines:
                        break
                    time.sleep(.05)
                else:
                    raise RuntimeError("Explorer did not render mounted file rows")
                def wait_exact(marker, count=1):
                    deadline=time.monotonic()+40
                    while time.monotonic()<deadline and proc.poll() is None:
                        lines=uart.read_bytes().splitlines()
                        require(not any(b" FAIL " in x for x in lines),"guest failure navigating Explorer")
                        if lines.count(marker)>=count:
                            return
                        time.sleep(.05)
                    raise RuntimeError("missing navigation marker "+repr(marker))
                wait_exact(b"SFS42 ROOT ROWS")
                d.key("down")
                d.key("ret")
                wait_exact(b"SFS42 ENTER DIRECTORY")
                wait_exact(b"SFS42 CHILD ROWS")
                d.key("backspace")
                wait_exact(b"SFS42 UP DIRECTORY")
                wait_exact(b"SFS42 ROOT ROWS",2)
            d.call("stop")
            o, final = snapshot("final")
            verify_ready(o)
            bar = [x for bus in o["pci"] for x in bus["devices"] if x.get("qdev_id")==CONTROLLER][0]["regions"]
            require(len(bar)==1,"BAR count")
            verify_bar(o["pci"],o["mtree"],bar[0]["address"])
            rows = [x for x in uart.read_bytes().splitlines() if x.startswith(b"SFS39 " if live else b"SFS34 ")]
            expected=[receipt.encode()]
            if live and receipt=="SFS39 MOUNT PASS":
                expected += [b"SFS39 DESKTOP READ PASS",b"SFS39 INPUT READ PASS"]
            require(rows==expected, "unexpected guest result "+repr(rows))
            if live:
                require(b"STYLEOS DESKTOP READY" in uart.read_bytes() and b"STYLEOS NATIVE RUNTIME READY" in uart.read_bytes(),"live desktop missing")
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
    tool=out/"paged-fixture"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-I",ROOT/"include",
        ROOT/"tools/stylefs_paged_fixture_34.c",
        *[ROOT/("kernel/"+s+".c") for s in ("stylefs_paged_stream64","stylefs_paged_pair64","stylefs_inode_root64","stylefs_inode_pages64","stylefs_namespace64","stylefs_dircommit64","stylefs_directory64","stylefs_alloc64","stylefs_map64","stylefs_extent64","block_service")],"-o",tool)
    volume=out/"provision.raw";run(tool,"new",volume)
    iso=build(out/"build",N)
    raw=out/"disk.raw";fixture(raw,N,volume)
    results=[]
    for name,receipt,writes in (("write","SFS34 WRITE PASS",46*512),("cold","SFS34 COLD PASS",0)):
        results.append(boot(out/name,iso,raw,N,SERIAL,receipt,writes,fw))
        require(raw.stat().st_size==N*512 and read_sector(raw,0)==TAG and read_sector(raw,BOUNDARY-1)==SENTINEL,"protected disk changed")
        export_check(tool,raw,out/name)
    raw.unlink()
    for name,n,serial,tag,corrupt,receipt in (
        ("foreign-serial",N,"FOREIGN-SFS34",TAG,False,"SFS34 REFUSE identity"),
        ("foreign-tag",N,SERIAL,b"X"*512,False,"SFS34 REFUSE tag"),
        ("wrong-capacity",N//2,SERIAL,TAG,False,"SFS34 REFUSE identity"),
        ("bad-metadata",N,SERIAL,TAG,True,"SFS34 REFUSE metadata")):
        raw=out/(name+".raw");fixture(raw,n,volume,tag,corrupt)
        before=hashlib.sha256(b"".join(read_sector(raw,x) for x in range(n-CAP,n))).digest()
        results.append(boot(out/name,iso,raw,n,serial,receipt,0,fw))
        require(before==hashlib.sha256(b"".join(read_sector(raw,x) for x in range(n-CAP,n))).digest() and read_sector(raw,BOUNDARY-1)==SENTINEL,"refused disk changed")
        raw.unlink()
    volume.unlink()
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("STYLEFS 34 PAGED GUEST PASS: real high-LBA mkdir, cancelled create, put, rename, cold reopen, host export and four refusals")
if __name__=="__main__":main()
