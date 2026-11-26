#!/usr/bin/env python3
"""Real regular-file backing, 64-bit offsets, actual byte persistence.
No physical device or claim of a complete >4GiB file workload."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--tool",type=Path,required=True)
    a=p.parse_args()
    tool=a.tool.resolve()
    def run(*args,ok=True):
        r=subprocess.run([str(tool),*map(str,args)],capture_output=True,text=True)
        if (r.returncode==0)!=ok:raise RuntimeError((args,r.returncode,r.stdout,r.stderr))
        return r.stdout
    # Small physical writes past 4GiB, without materializing all 6GiB.
    cap=6*1024*1024*1024//512
    start=4*1024*1024*1024//512+32
    with tempfile.TemporaryDirectory(prefix="stylefs-wide-") as tmp:
        root=Path(tmp);disk=root/"wide.raw";src=root/"source.bin";dst=root/"export.bin"
        data=bytes((i^(i>>9)^(i>>17))&255 for i in range(1024*1024+17))
        src.write_bytes(data)
        run("new",disk,cap,start)
        assert disk.stat().st_size==cap*512
        with disk.open("rb") as f:superblock=f.read(512)
        assert superblock[:8]==b"SF64JNL1"
        run("new",disk,cap,start,ok=False)
        run("stat",disk,start,1,ok=False)
        run("put",disk,start,1,0,src)
        assert f"bytes={len(data)}" in run("stat",disk,start,1)
        with disk.open("rb") as f:
            f.seek(start*512);assert f.read(len(data))==data
            assert f.read(495)==bytes(495)
        run("get",disk,start,1,dst);assert dst.read_bytes()==data
        run("get",disk,start,1,dst,ok=False);assert dst.read_bytes()==data
        run("put",disk,start,1,0,src,ok=False)
        run("put",disk,start,1,1,disk,ok=False)
        run("stat",disk,start+1,1,ok=False)
        assert src.read_bytes()==data
        # Replace with a distinct file, confirm retained old physical payload.
        new=root/"new.bin";new.write_bytes(b"replacement")
        run("put",disk,start,1,1,new)
        assert "revision=2 bytes=11" in run("stat",disk,start,1)
        result=root/"new-out.bin";run("get",disk,start,1,result)
        assert result.read_bytes()==b"replacement"
        with disk.open("rb") as f:
            assert f.read(512)==superblock
            f.seek(start*512);assert f.read(len(data))==data
        # First free sector after retained revision 1.
        at=(start+(len(data)+511)//512)*512
        with disk.open("r+b") as f:
            f.seek(at);assert f.read(11)==b"replacement"
            f.seek(at);f.write(b"X");f.flush();os.fsync(f.fileno())
        failed=root/"corrupt-export.bin"
        run("get",disk,start,1,failed,ok=False);assert not failed.exists()
        assert not list(root.glob("*.wide-*"))
        print("STYLEFS WIDE IMAGE PASS: 6GiB sparse geometry, physical offset >4GiB, actual 1MiB+17 bytes, reopen/replacement, source and old revision preserved, CRC refusal, no overwrite")
        print("source SHA256",hashlib.sha256(data).hexdigest())
if __name__=="__main__":main()
