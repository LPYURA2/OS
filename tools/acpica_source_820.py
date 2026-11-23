#!/usr/bin/env python3
"""Validate pinned real ACPICA sources before compiling any upstream code."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

VERSION = "20260930"
SHA256 = "aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5"
REQUIRED = (
 "source/include/acpi.h", "source/include/acpiosxf.h",
 "source/include/acexcep.h", "source/include/acenv.h",
 "source/components/utilities/utxfinit.c",
 "source/components/namespace/nsxfname.c",
 "source/components/parser/psparse.c",
)

class SourceRejected(ValueError):
 pass

def validate(path, expected_sha=SHA256, out_dir=None):
 path=Path(path)
 if not path.is_file() or path.stat().st_size>5000000:
  raise SourceRejected("missing or oversized archive")
 with path.open("rb") as f: digest=hashlib.file_digest(f,"sha256").hexdigest()
 if digest!=expected_sha: raise SourceRejected("upstream SHA-256 mismatch")
 mapped={}; total=0
 with tarfile.open(path,"r:gz") as tar:
  members=tar.getmembers()
  if len(members)>4000: raise SourceRejected("too many members")
  for member in members:
   if not (member.isfile() or member.isdir()):
    raise SourceRejected("link, device or special member")
   name=member.name
   if name.startswith("/") or "\\" in name:
    raise SourceRejected("absolute or backslash path")
   parts=PurePosixPath(name).parts
   if ".." in parts: raise SourceRejected("path traversal")
   if "source" not in parts: continue
   rel="/".join(parts[parts.index("source"):])
   if not rel.startswith(("source/include/","source/components/")) or member.isdir():
    continue
   if rel in mapped: raise SourceRejected("duplicate path")
   if member.size<1 or member.size>1000000: raise SourceRejected("bad file size")
   total+=member.size
   if total>15000000: raise SourceRejected("expanded source limit")
   if rel.endswith((".c",".h")): mapped[rel]=member
  missing=set(REQUIRED)-set(mapped)
  if missing: raise SourceRejected("missing ACPICA sources: "+str(sorted(missing)))
  count_c=sum(k.endswith(".c") for k in mapped)
  count_h=sum(k.endswith(".h") for k in mapped)
  if count_c<75 or count_h<30: raise SourceRejected("incomplete ACPICA core")
  for rel,anchor in (
     ("source/include/acpiosxf.h",b"AcpiOsMapMemory"),
     ("source/components/utilities/utxfinit.c",b"AcpiInitializeSubsystem")):
   stream=tar.extractfile(mapped[rel])
   if stream is None or anchor not in stream.read():
    raise SourceRejected("unexpected official source")
  if out_dir is not None:
   dest=Path(out_dir);dest.mkdir(parents=True,exist_ok=True)
   for rel,member in sorted(mapped.items()):
    target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True)
    stream=tar.extractfile(member)
    if stream is None: raise SourceRejected("source disappeared")
    with stream,target.open("wb") as output: output.write(stream.read())
 return {"stage":820,"version":VERSION,"archive_sha256":digest,
   "source_c":count_c,"source_headers":count_h,"all_sources_verified":True,
   "guest_acpica_linked":False,"dma_authorized":False,"foundation_complete":False}

def main():
 p=argparse.ArgumentParser()
 p.add_argument("--archive",required=True)
 p.add_argument("--output",required=True)
 p.add_argument("--extract-to")
 a=p.parse_args()
 result=validate(a.archive,out_dir=a.extract_to)
 path=Path(a.output);path.parent.mkdir(parents=True,exist_ok=True)
 path.write_text(json.dumps(result,sort_keys=True,indent=2)+"\n")
 print("STYLEOS S820 ACPICA SOURCE PASS "+VERSION+" SHA256="+result["archive_sha256"])

if __name__=="__main__":main()
