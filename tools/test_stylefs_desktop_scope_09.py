#!/usr/bin/env python3
"""Real post-EBS writable StyleFS v2 mount, isolated Q35 TCG profile."""
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
TAG=b"STYLEOS-SFS07-WRITABLE-V1".ljust(512,b"\0")
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
        subprocess.run([sys.executable,str(ROOT/"tools/build.py"),"--arch","x86_64","--desktop-owned-shell-865","--stylefs-mount-rw","--stylefs-desktop-scope","--output",str(build)],check=True,stdout=log,stderr=subprocess.STDOUT)
    iso=build/"x86_64/StyleOS-owned-shell-865.iso"
    manifest=json.loads((build/"x86_64/owned-shell-compiler.json").read_text())
    if manifest["profile"]!="stylefs-desktop-scope" or "-DSTYLEOS_STYLEFS_DESKTOP_SCOPE=1" not in manifest["flags"] or manifest["test_sources"]:raise ValueError("profile proof")
    tool=out/"stylefs-image"
    run("gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DSTYLEFS_HOST_FORMAT","-I",ROOT/"include",ROOT/"tools/stylefs_image.c",ROOT/"kernel/stylefs.c","-o",tool)
    base=out/"system.raw";run(tool,"new-system",base,"32")
    run(tool,"mkdir",base,"/Users/TestProfile")
    run(tool,"mkdir",base,"/Users/Other")
    payload=out/"private.txt";payload.write_bytes(b"private other user")
    run(tool,"put",base,"/Users/Other/private.txt",payload)
    if base.stat().st_size!=N*512:raise ValueError("volume capacity")
    with base.open("ab") as f:f.write(TAG)
    corrupt=out/"bad-tag.raw";shutil.copyfile(base,corrupt)
    with corrupt.open("r+b") as f:f.seek(N*512);f.write(b"X")
    initial={str(p):sha(p) for p in (base,corrupt,iso)}
    def boot(name,raw,serial,expected,write=False,reopen=False):
        raw_sha=sha(raw)
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
                    if len(s)!=1 or any(s[0].get(k)!=0 for k in ("unmap_bytes","unmap_operations")):raise ValueError("unexpected discard")
                    if not (write and label=="final") and any(s[0].get(k)!=0 for k in ("wr_bytes","wr_operations","flush_operations")):raise ValueError("unexpected disk mutation")
                    if write and label=="final" and not (s[0]["wr_operations"]>0 and s[0]["flush_operations"]>0):raise ValueError("no real writes/flush")
                    if any(s[0].get(k,0) for k in ("failed_wr_operations","invalid_wr_operations","failed_flush_operations","invalid_flush_operations")):raise ValueError("attempted write")
                    return o,s[0]
                snapshot("prelaunch");d.call("cont")
                if expected:
                    wait(proc,uart,expected,seconds=90)
                else:
                    d.wait_uart("STYLEFS MOUNT",timeout=90)
                    d.wait_uart("READY",timeout=90)
                    d.key("f8");d.screen("processes");d.key("f2");d.screen("root")
                    if write:
                        # Root mutation refused at the real filesystem boundary.
                        d.key("ctrl-n")
                        for k in ("n","o","p","e","ret"):d.key(k)
                        wait(proc,uart,b"STYLEOS SFS09 DENIED")
                        d.key("esc");d.call("stop");snapshot("denied-root");d.call("cont")
                    # Root remains visible; Users shows only the selected profile.
                    for _ in range(4):d.key("down")
                    d.key("ret");d.key("down");d.key("ret")
                    if write:
                        d.key("ctrl-n")
                        for k in ("l","a","b","ret"):d.key(k)
                        d.wait_uart("STYLEFS MKDIR",timeout=90)
                        d.key("f5");d.key("ctrl-n");d.key("a");d.key("ctrl-s")
                        d.wait_uart("DOCUMENT SAVE",timeout=90)
                        d.screen("saved")
                    elif reopen:
                        # One directory and one note inside the allowed profile.
                        # Select the last row and open it through Explorer,
                        # not just enumerate the tree or extract on the host.
                        for _ in range(2):d.key("down")
                        d.key("ret");d.wait_uart("DOCUMENT OPEN",timeout=90)
                        d.screen("reopened")
                d.call("stop");o,stats=snapshot("final")
                verify_ready(o);verify_bar(o["pci"],o["mtree"],0x6060)
                d.call("quit")
                if proc.wait(timeout=30)!=0:raise ValueError("QEMU exit")
            finally:
                if proc.poll() is None:proc.kill();proc.wait()
        lines=uart.read_bytes().splitlines()
        if expected:
            if [l for l in lines if b" FAIL " in l]!=[expected] or any(l.startswith(b"STYLEOS SFS07 ADMIT") for l in lines):raise ValueError("foreign disk admitted")
        else:
            admission=b"STYLEOS SFS07 ADMIT device-sectors=0x0000000000010001 volume-sectors=0x0000000000010000 write=0x0000000000000001 refusals=0x0000000000000003"
            if lines.count(admission)!=1 or b"STYLEOS SFS09 SCOPE BOUND" not in lines or b"STYLEOS DESKTOP STYLEFS V2" not in lines:raise ValueError("real mount receipt")
            if [l for l in lines if l.startswith(b"STYLEOS SFS06 DIRECTORY ")]!=[b"STYLEOS SFS06 DIRECTORY "+n for n in DIRS]:raise ValueError("real root entries")
            if not lines.index(admission)<lines.index(b"STYLEOS SFS06 ROOT BEGIN")<lines.index(b"STYLEOS SFS06 ROOT END"):raise ValueError("receipt order")
            if any(b" FAIL " in l for l in lines) or stats.get("rd_operations",0)<=0:raise ValueError("read proof")
            if reopen:
                receipt=[l for l in lines if l.startswith(b"STYLEOS S863S OPENED ")]
                if len(receipt)!=1 or not receipt[0].endswith(b" bytes=0x0000000000000001 hash=0xaf63dc4c8601ec8c"):
                    raise ValueError("cold guest note content")
        decoded=folder/"readback.raw";run(img,"convert","-f","qcow2","-O","raw",top,decoded)
        if sha(raw)!=raw_sha:raise ValueError("backing changed")
        if write:
            before=raw.read_bytes();after=decoded.read_bytes()
            if len(after)!=len(before) or after[:512]!=before[:512] or after[N*512:]!=before[N*512:]:raise ValueError("protected sector modified")
            commits=[int(l.split(b"lba=")[1],16) for l in lines if l.startswith(b"STYLEOS SFS07 COMMIT ")]
            if lines.count(b"STYLEOS SFS09 DENIED")!=1:raise ValueError("root deny receipt")
            if not commits or any(not 1<=n<N for n in commits) or stats["wr_operations"]!=len(commits) or stats["flush_operations"]!=len(commits) or stats["wr_bytes"]!=512*len(commits):raise ValueError("write/flush receipts")
        elif sha(decoded)!=raw_sha:raise ValueError("disk bytes changed")
        return {"name":name,"status":"PASS","uart_sha256":sha(uart),"disk_sha256":sha(decoded)}
    reports=[boot("valid",base,"STYLEOS-SFS07-RW",None,write=True)]
    changed=out/"valid/readback.raw"
    reports.append(boot("reopen",changed,"STYLEOS-SFS07-RW",None,reopen=True))
    volume=out/"verified-volume.raw"
    volume.write_bytes((out/"reopen/readback.raw").read_bytes()[:N*512])
    listing=subprocess.check_output([str(tool),"list",str(volume),"/Users/TestProfile"],text=True)
    if "d\t0\tlab" not in listing.splitlines():raise ValueError("missing persistent directory")
    notes=[l.split("\t",2)[2] for l in listing.splitlines() if l.startswith("f\t") and l.endswith(".txt")]
    if len(notes)!=1:raise ValueError("missing persistent note")
    run(tool,"get",volume,"/Users/TestProfile/"+notes[0],out/"note.txt")
    run(tool,"get",volume,"/Users/Other/private.txt",out/"other.txt")
    if (out/"other.txt").read_bytes()!=payload.read_bytes():raise ValueError("other data changed")
    root=subprocess.check_output([str(tool),"list",str(volume),"/"],text=True)
    if any(l.endswith("\tnope") or l.endswith("\tlab") for l in root.splitlines()):raise ValueError("root mutation")
    if (out/"note.txt").read_bytes()!=b"a":raise ValueError("persistent note bytes")
    reports.extend([boot("wrong-serial",base,"FOREIGN-SFS07",b"STYLEOS S850 FAIL sfs06-serial-capacity"),boot("wrong-tag",corrupt,"STYLEOS-SFS07-RW",b"STYLEOS S850 FAIL sfs06-tag")])
    if any(sha(Path(p))!=h for p,h in initial.items()):raise ValueError("source changed")
    (out/"results.json").write_text(json.dumps(reports,indent=2)+"\n")
    print("STYLEFS 09 GUEST SCOPE PASS")
if __name__=="__main__":main()
