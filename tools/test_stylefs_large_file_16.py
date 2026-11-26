#!/usr/bin/env python3
"""Full logical >4GiB file import and export, no sparse-write shortcut.
Source is sparse to save runner disk space; every byte is actually read,
written into StyleFS, flushed, read back, then extracted and hashed."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

SIZE=(1<<32)+17
START=32
CAP=6*1024*1024*1024//512
def hashes(path):
    h=hashlib.sha256();c=0;n=0
    with path.open("rb") as f:
        while chunk:=f.read(1024*1024):
            h.update(chunk);c=zlib.crc32(chunk,c);n+=len(chunk)
    return n,h.hexdigest(),c
def main():
    p=argparse.ArgumentParser();p.add_argument("--tool",type=Path,required=True);a=p.parse_args()
    tool=a.tool.resolve()
    with tempfile.TemporaryDirectory(prefix="stylefs-large-") as tmp:
        root=Path(tmp)
        if shutil.disk_usage(root).free<10*1024**3:raise RuntimeError("at least 10GiB free required for full payload and export")
        src=root/"source.bin";dst=root/"export.bin";disk=root/"wide.raw"
        with src.open("wb") as f:
            f.truncate(SIZE)
            # Unique nonzero markers throughout the logical stream and
            # on both sides of the 32-bit offset boundary.
            for off in range(0,SIZE,1024*1024):
                f.seek(off);f.write(struct.pack("<QQ",off,off^0xA55A123456789ABC))
            f.seek((1<<32)-8);f.write(b"crossing-4GiB-marker-data!")
            f.truncate(SIZE);f.flush();os.fsync(f.fileno())
        expected=hashes(src);assert expected[0]==SIZE
        print("SOURCE",expected,flush=True)
        def run(*args):
            subprocess.run([str(tool),*map(str,args)],check=True)
        run("new",disk,CAP,START)
        run("put",disk,START,1,0,src)
        stat=subprocess.check_output([str(tool),"stat",str(disk),str(START),"1"],text=True)
        assert f"bytes={SIZE}" in stat and f"crc={expected[2]:08x}" in stat
        print("COMMITTED",stat.strip(),flush=True)
        # A new process mounts the image read-only before extracting.
        run("get",disk,START,1,dst)
        assert hashes(dst)==expected and hashes(src)==expected
        with disk.open("rb") as f:
            f.seek(START*512+(1<<32)-8)
            assert f.read(25)==b"crossing-4GiB-marker-data!"
        print("STYLEFS FULL LARGE FILE PASS",SIZE,"bytes",expected[1],flush=True)
if __name__=="__main__":main()
