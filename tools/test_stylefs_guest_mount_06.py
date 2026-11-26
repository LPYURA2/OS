#!/usr/bin/env python3
"""Real post-EBS read-only StyleFS v2 mount, isolated Q35 TCG profile."""
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path
from test import ROOT, firmware_path
from test_desktop_v0 import Desktop
from test_desktop_interactive_863 import wait
from block_fixture_856_verify import CONTROLLER, DEVICE, controller_identity, tree_device
from guest_fixture_857 import verify_legacy_io
from bmdma_857_verify import verify_bar
from legacy_ports_857 import verify_ready

N=65536
TAG=b"STYLEOS-SFS06-READONLY-V1".ljust(512,b"\0")
DIRS=[b"StyleOS",b"Program Files",b"ProgramData",b"Users",b"Recovery",b"Temp"]
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args):
    subprocess.run(list(map(str,args)),check=True)
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    q=shutil.which("qemu-system-x86_64");img=shutil.which("qemu-img")
    code=firmware_path("OVMF_CODE","OVMF_CODE");vars=firmware_path("OVMF_VARS","OVMF_VARS")
    if not(q and img and code and vars):raise RuntimeError("QEMU and OVMF required")
    if not subprocess.check_output([q,"--version"],text=True).startswith("QEMU emulator version 8.2.2 "):raise ValueError("unvalidated fixture")
    build=out/"build"
    with (out/"compile.log").open("w") as log:
        subprocess.run([sys.executable,str(ROOT/"tools/build.py"),"--arch","x86_64","--desktop-owned-shell-865","--stylefs-mount-ro","--output",str(build)],check=True,stdout=log,stderr=subprocess.STDOUT)
    iso=build/"x86_64/StyleOS-owned-shell-865.iso"
    manifest=json.loads((build/"x86_64/owned-shell-compiler.json").read_text())
    if manifest["profile"]!="stylefs-mount-ro" or "-DSTYLEOS_STYLEFS_MOUNT_RO=1" not in manifest["flags"] or manifest["test_sources"]:raise ValueError("profile proof")
    tool=out/"stylefs-image"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-I",ROOT/"include",ROOT/"tools/stylefs_image.c",ROOT/"kernel/stylefs.c","-o",tool)
    base=out/"system.raw";run(tool,"new-system",base,"32")
    if base.stat().st_size!=N*512:raise ValueError("volume capacity")
    with base.open("ab") as f:f.write(TAG)
    corrupt=out/"bad-tag.raw";shutil.copyfile(base,corrupt)
    with corrupt.open("r+b") as f:f.seek(N*512);f.write(b"X")
    initial={str(p):sha(p) for p in (base,corrupt,iso)}
    def boot(name,raw,serial,expected):
        folder=out/name;folder.mkdir();uart=folder/"serial.log";top=folder/"data.qcow2"
        run(img,"create","-f","qcow2","-F","raw","-b",raw,top)
        shutil.copyfile(vars,folder/"OVMF_VARS.fd")
        cmd=[q,"-machine","q35","-accel","tcg,thread=single","-smp","1","-rtc","base=2026-11-25T12:34:00,clock=vm","-m","128M","-display","none","-vga","std","-serial",f"file:{uart}","-monitor","none","-qmp","stdio","-net","none","-no-reboot","-S","-drive",f"if=pflash,format=raw,readonly=on,file={code}","-drive",f"if=pflash,format=raw,file={folder/'OVMF_VARS.fd'}","-boot","order=d,strict=on","-cdrom",str(iso),"-device","piix3-ide,id=styleos-ide,bus=pcie.0,addr=0x5"]
        for node in ({"driver":"file","filename":str(raw),"node-name":"s856-file","read-only":True},{"driver":"raw","file":"s856-file","node-name":"s856-raw","read-only":True},{"driver":"file","filename":str(top),"node-name":"s856-top-file","read-only":False},{"driver":"qcow2","file":"s856-top-file","backing":"s856-raw","node-name":"s856-top","read-only":False}):cmd+=["-blockdev",json.dumps(node)]
        cmd+=["-device",f"ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive=s856-top,serial={serial}"]
        (folder/"command.json").write_text(json.dumps(cmd))
        with (folder/"qemu.log").open("wb") as err:
            proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err);d=Desktop(proc,uart,folder)
            try:
                if "QMP" not in d.receive():raise ValueError("QMP")
                d.call("qmp_capabilities")
                def snapshot(label):
                    o={}
                    for key,method,kw in [("status","query-status",{}),("pci","query-pci",{}),("blocks","query-block",{}),("nodes","query-named-block-nodes",{}),("qtree","human-monitor-command",{"command-line":"info qtree"}),("mtree","human-monitor-command",{"command-line":"info mtree -f -o"}),("stats","query-blockstats",{"query-nodes":False})]:
                        o[key]=d.call(method,**kw)
                    (folder/(label+".json")).write_text(json.dumps(o,indent=2))
                    if o["status"].get("running") is not False:raise ValueError("not paused")
                    dev=[v for bus in o["pci"] if bus.get("bus")==0 for v in bus["devices"] if v.get("qdev_id")==CONTROLLER]
                    if len(dev)!=1 or not controller_identity(dev[0]):raise ValueError("controller")
                    tree_device(o["qtree"]);verify_legacy_io(o["mtree"])
                    bs=[v for v in o["blocks"] if v.get("qdev") in (DEVICE,"/machine/peripheral/"+DEVICE)]
                    if len(bs)!=1 or bs[0]["inserted"]["node-name"]!="s856-top" or bs[0]["inserted"]["image"]["virtual-size"]!=(N+1)*512:raise ValueError("disk")
                    nodes={v["node-name"]:v for v in o["nodes"] if v.get("node-name") in ("s856-raw","s856-file","s856-top","s856-top-file")}
                    if set(nodes)!={"s856-raw","s856-file","s856-top","s856-top-file"} or any(nodes[k].get("ro") is not True or nodes[k].get("file")!=str(raw) for k in ("s856-raw","s856-file")):raise ValueError("immutable backing")
                    s=[v["stats"] for v in o["stats"] if v.get("qdev") in (DEVICE,"/machine/peripheral/"+DEVICE)]
                    if len(s)!=1 or any(s[0].get(k)!=0 for k in ("wr_bytes","wr_operations","flush_operations","unmap_bytes","unmap_operations")):raise ValueError("disk mutation")
                    if any(s[0].get(k,0) for k in ("failed_wr_operations","invalid_wr_operations","failed_flush_operations","invalid_flush_operations")):raise ValueError("attempted write")
                    return o,s[0]
                snapshot("prelaunch");d.call("cont")
                if expected:
                    wait(proc,uart,expected,timeout=90)
                else:
                    d.wait_uart("STYLEFS MOUNT",timeout=90)
                    d.wait_uart("READY",timeout=90)
                    d.key("f8");d.screen("processes");d.key("f2");d.screen("root")
                d.call("stop");o,stats=snapshot("final")
                verify_ready(o);verify_bar(o["pci"],o["mtree"],0x6060)
                d.call("quit")
                if proc.wait(timeout=30)!=0:raise ValueError("QEMU exit")
            finally:
                if proc.poll() is None:proc.kill();proc.wait()
        lines=uart.read_bytes().splitlines()
        if expected:
            if [l for l in lines if b" FAIL " in l]!=[expected] or any(l.startswith(b"STYLEOS SFS06 ADMIT") for l in lines):raise ValueError("foreign disk admitted")
        else:
            admission=b"STYLEOS SFS06 ADMIT device-sectors=0x0000000000010001 volume-sectors=0x0000000000010000 write=0x0000000000000000 refusals=0x0000000000000003"
            if lines.count(admission)!=1 or b"STYLEOS DESKTOP STYLEFS V2" not in lines:raise ValueError("real mount receipt")
            if [l for l in lines if l.startswith(b"STYLEOS SFS06 DIRECTORY ")]!=[b"STYLEOS SFS06 DIRECTORY "+n for n in DIRS]:raise ValueError("real root entries")
            if not lines.index(admission)<lines.index(b"STYLEOS SFS06 ROOT BEGIN")<lines.index(b"STYLEOS SFS06 ROOT END"):raise ValueError("receipt order")
            if any(b" FAIL " in l for l in lines) or stats.get("rd_operations",0)<=0:raise ValueError("read proof")
        decoded=folder/"readback.raw";run(img,"convert","-f","qcow2","-O","raw",top,decoded)
        if sha(decoded)!=initial[str(raw)]:raise ValueError("disk bytes changed")
        return {"name":name,"status":"PASS","uart_sha256":sha(uart),"disk_sha256":sha(decoded)}
    reports=[boot("valid",base,"STYLEOS-SFS06-RO",None),boot("wrong-serial",base,"FOREIGN-SFS06",b"STYLEOS S850 FAIL sfs06-serial-capacity"),boot("wrong-tag",corrupt,"STYLEOS-SFS06-RO",b"STYLEOS S850 FAIL sfs06-tag")]
    if any(sha(Path(p))!=h for p,h in initial.items()):raise ValueError("source changed")
    (out/"results.json").write_text(json.dumps(reports,indent=2)+"\n")
    print("STYLEFS 06 REAL GUEST MOUNT PASS")
if __name__=="__main__":main()
