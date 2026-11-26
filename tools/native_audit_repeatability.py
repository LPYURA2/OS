#!/usr/bin/env python3
"""Repeat the unchanged native guest runners under measured host concurrency.
Every failed attempt is retained and makes this diagnostic campaign fail.
No guest deadlines, input sequence, verifier or kernel policy is changed.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
COMMON = ["--tasks", "--mixed", "--four", "--iso", "--provisioned",
          "--integrated", "--owned-shell"]
PROFILES = {
    "timer": ["--native-catalog", "--native-timer-load"],
    "hang": ["--native-catalog", "--native-hang"],
    "stop": ["--native-catalog", "--native-forced-stop"],
    "divide": ["--native-catalog", "--native-fault-probe", "divide"],
    "desktop": ["--extended-desktop"],
}
MARKERS = (b"STYLEOS S863 READY ", b"DATA MOUNT", b"STYLEOS DESKTOP DOCUMENT SAVE",
           b"STYLEOS DESKTOP DOCUMENT OPEN", b"STYLEOS NATIVE STOP-DONE ",
           b"STYLEOS NATIVE EMPTY", b"STYLEOS S850 FAIL", b"STYLEOS NATIVE FAIL")

def read(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None

def dump(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")

def host():
    # cgroup files are optional on Linux; absence is recorded, not imputed.
    return {"loadavg": read("/proc/loadavg"),
            "cpu": (read("/proc/stat") or "").splitlines()[:1],
            "pressure_cpu": read("/proc/pressure/cpu"),
            "pressure_io": read("/proc/pressure/io"),
            "cgroup_cpu_max": read("/sys/fs/cgroup/cpu.max"),
            "cgroup_cpu_stat": read("/sys/fs/cgroup/cpu.stat"),
            "cgroup_memory_events": read("/sys/fs/cgroup/memory.events"),
            "cgroup_memory_current": read("/sys/fs/cgroup/memory.current")}

def observe(item, now):
    # Incremental UART observation; timestamps are host observation times
    # (sample resolution), never guest event timestamps.
    for uart in sorted(item["directory"].glob("*/serial.log")):
        rel = str(uart.relative_to(item["directory"]))
        offset, pending = item["offsets"].get(rel, (0, b""))
        with uart.open("rb") as stream:
            stream.seek(offset)
            data = pending + stream.read()
            end = stream.tell()
        lines = data.split(b"\n")
        item["offsets"][rel] = (end, lines.pop())
        for marker in MARKERS:
            key = (rel, marker)
            if key not in item["seen"] and any(marker in line for line in lines):
                item["seen"].add(key)
                item["events"].append({"seconds": round(now-item["start"], 3),
                    "serial": rel, "marker": marker.decode("ascii")})

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--mode", choices=("sequential", "parallel"), required=True)
    p.add_argument("--rounds", type=int, default=1, choices=(1, 2))
    args = p.parse_args()
    if not sys.platform.startswith("linux") or not hasattr(os, "sched_getaffinity"):
        p.error("Linux CPU affinity is required")
    before = sorted(os.sched_getaffinity(0))
    if len(before) < 2:
        p.error("two allowed logical CPUs are required")
    os.sched_setaffinity(0, set(before[:2]))
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    dump(out/"environment.json", {"mode": args.mode, "rounds": args.rounds,
        "original_affinity": before, "test_affinity": sorted(os.sched_getaffinity(0)),
        "host": host(), "sha": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "python": sys.version, "qemu": subprocess.check_output(
            ["qemu-system-x86_64", "--version"], text=True).splitlines()[0],
        "sampling_seconds": 2, "profiles": PROFILES})
    results = []
    active = []
    def launch(wave, name):
        prefix = out/f"round-{wave}-{name}"
        cmd = [sys.executable, str(ROOT/"tools/test_desktop_storage_service_863.py"),
               *COMMON, *PROFILES[name], "--output", str(prefix)]
        log = (out/f"round-{wave}-{name}.log").open("wb")
        try:
            proc = subprocess.Popen(cmd, cwd=ROOT, stdout=log,
                                    stderr=subprocess.STDOUT, start_new_session=True)
        finally:
            log.close()
        item = {"wave": wave, "name": name, "directory": prefix, "proc": proc,
                "start": time.monotonic(), "cmd": cmd, "events": [],
                "offsets": {}, "seen": set()}
        active.append(item)
        print(f"START round={wave} profile={name} pid={proc.pid}", flush=True)

    def collect(telemetry):
        while active:
            now = time.monotonic()
            for item in active:
                observe(item, now)
            telemetry.write(json.dumps({"monotonic": now, "host": host(),
                "active": [{"name": i["name"], "pid": i["proc"].pid,
                    "elapsed": round(now-i["start"], 3)} for i in active]}) + "\n")
            telemetry.flush()
            for item in active[:]:
                rc = item["proc"].poll()
                if rc is None:
                    continue
                observe(item, time.monotonic())
                result = {"round": item["wave"], "profile": item["name"],
                    "command": item["cmd"], "returncode": rc,
                    "elapsed_seconds": round(time.monotonic()-item["start"], 3),
                    "markers": item["events"]}
                proof = item["directory"]/"results.json"
                try:
                    result["runner_status"] = json.loads(proof.read_text()).get("status")
                except (OSError, ValueError):
                    result["runner_status"] = None
                result["pass"] = rc == 0 and result["runner_status"] == "PASS"
                results.append(result)
                dump(out/"attempts.json", results)
                active.remove(item)
                print(f"{'PASS' if result['pass'] else 'FAIL'} round={item['wave']} "
                      f"profile={item['name']} rc={rc} seconds={result['elapsed_seconds']}",
                      flush=True)
            if active:
                time.sleep(2)
    try:
        with (out/"host-samples.jsonl").open("w") as telemetry:
            for wave in range(1, args.rounds+1):
                for name in PROFILES:
                    launch(wave, name)
                    if args.mode == "sequential":
                        collect(telemetry)
                collect(telemetry)
    finally:
        # A wrapper interruption must not leave a guest running.
        for item in active:
            try:
                os.killpg(item["proc"].pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            item["proc"].wait()
    passed = sum(r["pass"] for r in results)
    expected = args.rounds*len(PROFILES)
    dump(out/"summary.json", {"expected": expected, "completed": len(results),
         "passed": passed, "failed": len(results)-passed,
         "status": "PASS" if passed == expected else "FAIL"})
    return 0 if passed == expected else 1

if __name__ == "__main__":
    raise SystemExit(main())
