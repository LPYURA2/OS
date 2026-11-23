#!/usr/bin/env python3
"""Reject malicious ACPICA source bundles without network dependency."""
import hashlib,io,tarfile,tempfile
from pathlib import Path
from acpica_source_820 import REQUIRED,SourceRejected,validate
def make(path,extra=(),missing=(),duplicate=None):
 with tarfile.open(path,"w:gz") as tar:
  for name in REQUIRED:
   if name in missing:continue
   data=b"AcpiOsMapMemory AcpiInitializeSubsystem "*10
   info=tarfile.TarInfo("acpica-unix-20260930/"+name)
   info.size=len(data);tar.addfile(info,io.BytesIO(data))
  for name,kind in extra:
   info=tarfile.TarInfo(name)
   if kind=="link":
    info.type=tarfile.SYMTYPE;info.linkname="../../etc/passwd";tar.addfile(info)
   else:
    info.size=1;tar.addfile(info,io.BytesIO(b"x"))
  if duplicate:
   info=tarfile.TarInfo("acpica-unix-20260930/"+duplicate)
   info.size=1;tar.addfile(info,io.BytesIO(b"x"))
def rejected(path,digest=None):
 if digest is None:digest=hashlib.sha256(path.read_bytes()).hexdigest()
 try:validate(path,expected_sha=digest)
 except SourceRejected:return True
 return False
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/"fake.tar.gz"
 scenarios=[
  {"missing":(REQUIRED[0],)},
  {"extra":(("acpica/source/include/../../bad","file"),)},
  {"extra":(("/absolute/path","file"),)},
  {"extra":(("acpica/source/include/escape","link"),)},
  {"duplicate":REQUIRED[0]},
 ]
 for case in scenarios:
  make(p,**case);assert rejected(p),case
 make(p);assert rejected(p,"0"*64)
 assert rejected(p) # source-count lower bound
print("STYLEOS S820 NEGATIVE PASS 7/7")
