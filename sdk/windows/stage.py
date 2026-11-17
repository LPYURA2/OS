#!/usr/bin/env python3
"""Stage the x64 Windows SDK from verified upstream archives; no network."""
import argparse
import hashlib
from pathlib import Path
import shutil
import sys
import zipfile

SDK = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SDK))
from styleos_sdk import verify

LLVM_SHA = "171b298f46fa4b5653dc7b01cc5e709caca645991be8e1b0724466c4a282a1ac"
PYTHON_SHA = "90905e64a840557d8c1bc4f5b74db3d7ee85e3ffb217611cac1c7813acea4159"
LLVM_PREFIX = "llvm-mingw-20251104-ucrt-x86_64/"
LLVM_FILES = ("bin/clang-21.exe", "bin/ld.lld.exe", "bin/libclang-cpp.dll",
              "bin/libLLVM-21.dll", "bin/libc++.dll", "bin/libunwind.dll",
              "LICENSE.TXT")


def require_sha(path, expected):
    with path.open("rb") as f:
        if hashlib.file_digest(f, "sha256").hexdigest() != expected:
            raise ValueError(f"unexpected upstream checksum: {path}")


def copy_member(zf, name, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with zf.open(name) as source, dest.open("xb") as out:
        shutil.copyfileobj(source, out)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--llvm-zip", required=True, type=Path)
    ap.add_argument("--python-zip", required=True, type=Path)
    ap.add_argument("--package", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    a = ap.parse_args()
    require_sha(a.llvm_zip, LLVM_SHA)
    require_sha(a.python_zip, PYTHON_SHA)
    metadata = verify(a.package)
    if metadata["project"]["id"] != "styleos.presentations":
        raise ValueError("the SDK bundle must include the Presentations example")
    a.output.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(a.llvm_zip) as llvm:
        files = list(LLVM_FILES)
        files.extend(p[len(LLVM_PREFIX):] for p in llvm.namelist()
                     if p.startswith(LLVM_PREFIX + "lib/clang/21/include/") and not p.endswith("/"))
        for f in files:
            copy_member(llvm, LLVM_PREFIX + f, a.output / "toolchain" / f)
    with zipfile.ZipFile(a.python_zip) as python:
        for f in python.namelist():
            if "/" in f or "\\" in f or f in (".", ".."):
                raise ValueError("unexpected embeddable Python archive member")
            copy_member(python, f, a.output / "runtime" / f)
    for name in ("include", "examples", "templates"):
        shutil.copytree(SDK / name, a.output / "sdk" / name)
    for name in ("README.md", "styleos_sdk.py"):
        shutil.copyfile(SDK / name, a.output / "sdk" / name)
    for cmd in SDK.glob("windows/*.cmd"):
        (a.output / cmd.name).write_bytes(cmd.read_text(encoding="utf-8").replace("\n", "\r\n").encode("ascii"))
    guide = SDK / "windows" / "START-HERE-RU.txt"
    (a.output / guide.name).write_bytes(guide.read_text(encoding="utf-8").encode("utf-8-sig"))
    shutil.copyfile(SDK / "windows" / "THIRD-PARTY.md", a.output / "THIRD-PARTY.md")
    (a.output / "artifacts").mkdir()
    shutil.copyfile(a.package, a.output / "artifacts" / "presentations.tea")
    print("STAGE PASS:", a.output.resolve())


if __name__ == "__main__":
    main()
