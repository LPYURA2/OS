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
def host_idle_sample(proc,seconds):
    # Linux /proc counts the entire QEMU process, not guest CPU time or RAM.
    import os
    def sample():
        require(proc.poll() is None,"QEMU exited during idle sample")
        stat=Path(f"/proc/{proc.pid}/stat").read_text().rsplit(")",1)[1].split()
        return time.monotonic_ns(),int(stat[11])+int(stat[12]),int(stat[21])
    hz=os.sysconf("SC_CLK_TCK")
    page=os.sysconf("SC_PAGE_SIZE")
    before=sample()
    time.sleep(seconds)
    after=sample()
    wall=(after[0]-before[0])/1e9
    cpu=(after[1]-before[1])/hz
    require(wall>=seconds and cpu>=0,"host timing accounting")
    return {"wall_seconds":wall,"qemu_cpu_seconds":cpu,
            "qemu_cpu_percent_one_host_core":100*cpu/wall,
            "qemu_rss_before_bytes":before[2]*page,"qemu_rss_after_bytes":after[2]*page}
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
def boot(folder, iso, raw, n, serial, receipt, expected_writes, fw, live=False, save44=False, cold44=False, namespace46=None, tea69=None):
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
                if namespace46=="create":
                    d.key("ctrl-n")
                    for key in ("z","dot","t","x","t"): d.key(key)
                    d.key("ret")
                    wait_exact(b"SFS46 CREATE PASS")
                    d.key("down")
                    d.key("ctrl-r")
                    for key in ("y","dot","t","x","t"): d.key(key)
                    d.key("ret")
                    wait_exact(b"SFS46 RENAME PASS")
                elif namespace46=="delete":
                    d.key("down")
                    d.key("delete")
                    d.key("esc")
                    d.key("delete")
                    d.key("ret")
                    wait_exact(b"SFS46 DELETE PASS")
                # Open the real nested text file, verify the editor's
                # content hash, then prove editing is blocked in RO mode.
                digest=14695981039346656037
                original=b"StyleOS: paged file read test.\nSecond line.\n"
                for byte in (b"a"+original if cold44 else original):
                    digest=((digest^byte)*1099511628211)&((1<<64)-1)
                d.key("ret")
                wait_exact(("SFS43 TEXT OPEN %016x"%digest).encode())
                if save44:
                    d.key("a")
                    d.key("ctrl-s")
                    wait_exact(b"SFS44 SAVE PASS")
                elif not cold44 and not namespace46:
                    d.key("a")
                    wait_exact(b"SFS43 EDIT REFUSED")
                d.key("f2")
                d.key("backspace")
                wait_exact(b"SFS42 UP DIRECTORY")
                wait_exact(b"SFS42 ROOT ROWS",2)
                if tea69 is not None:
                    # The new directory e follows a and d, and contains
                    # exactly one individually pinned package.
                    d.key("down");d.key("down");d.key("ret")
                    wait_exact(b"SFS42 ENTER DIRECTORY",2)
                    def wait_prefix(marker,count=1):
                        deadline=time.monotonic()+40
                        while time.monotonic()<deadline and proc.poll() is None:
                            rows=uart.read_bytes().splitlines()
                            require(not any(b" FAIL " in x for x in rows),"TEA guest failure")
                            matches=[x for x in rows if x.startswith(marker)]
                            if len(matches)>=count:return matches[count-1]
                            time.sleep(.05)
                        raise RuntimeError("missing TEA receipt "+repr(marker))
                    if tea69:
                        from native_desktop_acceptance import events
                        from uart_receipts import complete_lines
                        from native_forced_stop_verify import records
                        from statistics import median
                        seen_pids=set()
                        seen_windows=set()
                        timings=[]
                        expected=[]
                        idle={"before_cycles":host_idle_sample(proc,60)}
                        for cycle in range(1,101):
                            if cycle>1:
                                # F2 clears stale native focus and restores Explorer.
                                d.key("f2")
                            started=time.monotonic_ns()
                            d.key("ret")
                            file=wait_prefix(b"STYLEOS TEA69 STYLEFS id=",cycle)
                            require(b" id=0x0000000000000006 " in file and b" revision=0x0000000000000001 " in file,"exact selected file")
                            launch=wait_prefix(b"STYLEOS TEA69 LAUNCHED ",cycle)
                            frame=wait_prefix(b"STYLEOS NATIVE FRAME ",cycle)
                            pid=launch.split()[3]
                            pid_num=int(pid.split(b"=")[1],16)
                            window=launch.split()[-1]
                            require(pid in frame.split() and pid_num not in seen_pids and window not in seen_windows,"fresh application identity")
                            seen_pids.add(pid_num);seen_windows.add(window)
                            wait_prefix(b"STYLEOS TEA70 RESOURCES ",2*cycle-1)
                            observed=events(complete_lines(uart.read_bytes()))
                            require(bool(observed),"guest pointer coordinates unavailable")
                            d.x,d.y=observed[-1]["x"],observed[-1]["y"]
                            count=len(observed)
                            d.click(350,130)
                            deadline=time.monotonic()+40
                            while time.monotonic()<deadline and proc.poll() is None:
                                observed=events(complete_lines(uart.read_bytes()))
                                if len(observed)>count and (observed[-1]["x"],observed[-1]["y"])==(350,130) and not observed[-1]["buttons"]:
                                    break
                                time.sleep(.05)
                            else:
                                raise RuntimeError("native focus click receipt missing")
                            if cycle in (1,100):
                                visible=d.screen("tea69-file-visible" if cycle==1 else "tea70-file-visible-last")
                                visible.check(281,81,"976332");visible.check(439,199,"976332")
                            input_ready=time.monotonic_ns()
                            if cycle==1:
                                def received(kind,code):
                                    deadline=time.monotonic()+40
                                    while time.monotonic()<deadline and proc.poll() is None:
                                        rows=records(complete_lines(uart.read_bytes()),b"STYLEOS TEA70 INPUT-COUNTS",
                                            ("pid","gen","empty","delivered","other","kind","code"))
                                        matches=[v for _,v in rows if v["pid"]==pid_num and v["kind"]==kind and v["code"]==code]
                                        if matches:return matches[-1]
                                        time.sleep(.05)
                                    raise RuntimeError("missing authenticated input counter")
                                # Actual consumption of pointer release, not merely QMP submission.
                                poll_before=received(3,0)
                                poll_start=time.monotonic_ns()
                                idle["one_app_no_input"]=host_idle_sample(proc,10)
                            closed_start=time.monotonic_ns()
                            d.key("q")
                            retired=wait_prefix(b"STYLEOS TEA69 RETIRED ",cycle)
                            require(pid in retired.split() and window in retired.split(),"exact process retirement")
                            wait_prefix(b"STYLEOS TEA70 RESOURCES ",2*cycle)
                            ended=time.monotonic_ns()
                            if cycle==1:
                                poll_after=received(1,0x10)
                                require(poll_after["gen"]==poll_before["gen"] and
                                        poll_after["empty"]>=poll_before["empty"] and
                                        poll_after["delivered"]==poll_before["delivered"]+1 and
                                        poll_after["other"]==poll_before["other"],"idle input counters")
                                elapsed=(ended-poll_start)/1e9
                                delta=poll_after["empty"]-poll_before["empty"]
                                require(delta<=1,"more than one empty request during event wait")
                                lines=complete_lines(uart.read_bytes())
                                parks=records(lines,b"STYLEOS TEA70 PARK",("pid","gen"))
                                wakes=records(lines,b"STYLEOS TEA70 WAKE",("pid","gen"))
                                require(any(v["pid"]==pid_num for _,v in parks) and any(v["pid"]==pid_num for _,v in wakes),"actual park/wake receipts missing")
                                idle["guest_input_poll_baseline"]={"before":poll_before,"after":poll_after,
                                    "empty_requests":delta,"host_observed_seconds":elapsed,
                                    "empty_requests_per_host_second":delta/elapsed,
                                    "scope":"same process between consumed release and Q, includes host boundary overhead; not guest CPU usage"}
                            expected.extend([(1,pid_num,1,3,1,1,1),(2,pid_num,0,0,0,0,0)])
                            resources=records(complete_lines(uart.read_bytes()),b"STYLEOS TEA70 RESOURCES",
                                ("phase","pid","resident","pages","owners","windows","ports"))
                            require([tuple(v.values()) for _,v in resources]==expected,"StyleFS TEA resource release")
                            timings.append({"cycle":cycle,"launch_to_input_ms":(input_ready-started)/1e6,
                                            "close_to_release_ms":(ended-closed_start)/1e6})
                        idle["after_cycles"]=host_idle_sample(proc,60)
                        (folder/"host-idle-baseline.json").write_text(json.dumps({
                            "scope":"Linux host QEMU TCG process only; not guest CPU or RAM; no input during samples",
                            "samples":idle},indent=2)+"\n")
                        def stats(key):
                            values=sorted(t[key] for t in timings)
                            return {"median":median(values),"p95":values[94],"max":values[-1]}
                        (folder/"native-resource-accounting.json").write_text(json.dumps({
                            "scope":"native leased user pages only; QEMU host timing includes QMP/input and occasional screenshot",
                            "page_bytes":4096,"cycles":100,"snapshots":[v for _,v in resources],
                            "timings":timings,"launch_to_input_ms":stats("launch_to_input_ms"),
                            "close_to_release_ms":stats("close_to_release_ms")},indent=2)+"\n")
                        # A separate 101st application is forcibly stopped
                        # while parked, through the actual two-step Processes UI.
                        # Keep the 100 normal lifecycle measurements intact.
                        d.key("f2");d.key("ret")
                        launch=wait_prefix(b"STYLEOS TEA69 LAUNCHED ",101)
                        pid_num=int(launch.split()[3].split(b"=")[1],16)
                        gen=int(launch.split()[4].split(b"=")[1],16)
                        require(pid_num not in seen_pids,"fresh stop-test PID")
                        deadline=time.monotonic()+40
                        while time.monotonic()<deadline and proc.poll() is None:
                            lines=complete_lines(uart.read_bytes())
                            parks=records(lines,b"STYLEOS TEA70 PARK",("pid","gen"))
                            if any(v=={"pid":pid_num,"gen":gen} for _,v in parks):break
                            time.sleep(.05)
                        else:raise RuntimeError("stop-test app never parked")
                        snapshot_count=sum(x==b"STYLEOS PROCESSES END" for x in lines)
                        d.key("f8");wait_exact(b"STYLEOS PROCESSES END",snapshot_count+1)
                        from native_process_view_02 import decimal,ROW,HEADER,END
                        lines=complete_lines(uart.read_bytes())
                        end=max(i for i,x in enumerate(lines) if x==END)
                        start=max(i for i,x in enumerate(lines[:end]) if x.startswith(HEADER+b" "))
                        values=[decimal(x,ROW,(b"pid",b"group",b"state",b"ticks",b"bytes")) for x in lines[start+1:end]]
                        require(len(values)==1 and values[0][0]==pid_num and values[0][2]==3,"Processes must show actual waiting PID")
                        d.key("down");d.key("delete")
                        arm=wait_prefix(b"STYLEOS PROCESSES ARM ")
                        require(arm==f"STYLEOS PROCESSES ARM pid={pid_num}".encode(),"confirmation target mismatch")
                        # Wait for the Delete input/render boundary before Enter.
                        deadline=time.monotonic()+40
                        while time.monotonic()<deadline and proc.poll() is None:
                            lines=complete_lines(uart.read_bytes())
                            ev=events(lines)
                            if ev and ev[-1]["key"]==0x153 and ev[-1]["writes"]>0:break
                            time.sleep(.05)
                        else:raise RuntimeError("stop confirmation not rendered")
                        d.key("ret")
                        stop=wait_prefix(b"STYLEOS PROCESSES STOP ")
                        require(stop==f"STYLEOS PROCESSES STOP pid={pid_num} status=0 remaining=0".encode(),"waiting process stop refusal")
                        wait_prefix(b"STYLEOS TEA69 RETIRED ",101)
                        wait_prefix(b"STYLEOS TEA70 RESOURCES ",202)
                        lines=complete_lines(uart.read_bytes())
                        resources=records(lines,b"STYLEOS TEA70 RESOURCES",("phase","pid","resident","pages","owners","windows","ports"))
                        require([tuple(v.values()) for _,v in resources]==expected+[(1,pid_num,1,3,1,1,1),(2,pid_num,0,0,0,0,0)],"forced waiting cleanup")
                        require(not any(v["pid"]==pid_num for _,v in records(lines,b"STYLEOS TEA70 WAKE",("pid","gen"))),"stop-test app woke unexpectedly")
                        (folder/"waiting-stop.json").write_text(json.dumps({"pid":pid_num,"gen":gen,"state_before":3,"status":0,"resources":[v for _,v in resources[-2:]]},indent=2)+"\n")
                    else:
                        d.key("ret")
                        wait_exact(b"STYLEOS TEA69 STYLEFS REFUSED")
                        # Another ordinary input must still be processed.
                        d.key("backspace");wait_exact(b"SFS42 UP DIRECTORY",2)
                        rows=uart.read_bytes().splitlines()
                        require(not any(x.startswith((b"STYLEOS TEA69 LAUNCHED ",b"STYLEOS NATIVE FRAME ")) for x in rows),"invalid package executed")
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
            written=final["wr_bytes"]-start["wr_bytes"]
            require(written>0 if save44 or namespace46 in ("create","delete") else written==expected_writes, "write byte count")
            require(final["unmap_bytes"]==start["unmap_bytes"], "unexpected unmap")
            if expected_writes or save44 or namespace46 in ('create','delete'):
                require(final["flush_operations"]-start["flush_operations"]>=2, "flush evidence")
            for k in ("failed_wr_operations","invalid_wr_operations","failed_flush_operations","invalid_flush_operations"):
                require(final.get(k,0)==start.get(k,0), k)
            d.call("quit")
            require(proc.wait(timeout=20)==0, "QEMU exit")
        finally:
            d.ready.close()
            if proc.poll() is None:
                proc.kill(); proc.wait()
    return {"receipt":receipt,"written_bytes":written,"sectors":n}

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
