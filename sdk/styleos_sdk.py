#!/usr/bin/env python3
"""Build and inspect the first experimental StyleOS application packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

SDK = Path(__file__).resolve().parent
FORMAT = "styleos.tea.sdk-v0"
INTERFACE = "styleos.text-callback-v0"
ENTRY = "styleos_app_text_v0"
ARCH = "x86_64"
MACHINE = 62
MANIFEST = "manifest.json"
PAYLOAD = "payload.elf"
MAX_PACKAGE = 2 * 1024 * 1024
MAX_PAYLOAD = 1024 * 1024
MAX_MANIFEST = 4096
PROJECT_KEYS = {"id", "name", "version"}


def strict_json(data):
    def distinct(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8"), object_pairs_hook=distinct)


def project_fields(data):
    if not isinstance(data, dict) or set(data) != PROJECT_KEYS:
        raise ValueError("app.json must contain id, name and version")
    ident, name, version = (data[x] for x in ("id", "name", "version"))
    if not isinstance(ident, str) or len(ident) > 64 or not re.fullmatch(
            r"[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+", ident):
        raise ValueError("id must look like example.my-app and be at most 64 characters")
    if (not isinstance(name, str) or not name.strip() or len(name) > 80 or
            any(not c.isprintable() for c in name)):
        raise ValueError("name must be printable text, 1–80 characters")
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]{1,4}\.[0-9]{1,4}\.[0-9]{1,4}", version):
        raise ValueError("version must look like 0.1.0")
    return data


def elf_entry(data):
    """Identify this SDK's x86-64 executable format, not a full ELF loader."""
    if not 64 <= len(data) <= MAX_PAYLOAD or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("expected a bounded little-endian ELF64 payload")
    kind, machine, version, entry, phoff, _shoff, _flags, ehsize, phsize, phnum = (
        struct.unpack_from("<HHIQQQIHHH", data, 16)
    )
    if kind != 2 or machine != MACHINE or version != 1 or ehsize != 64 or phsize != 56:
        raise ValueError("unexpected ELF type, architecture or headers")
    if not 1 <= phnum <= 16 or phoff < 64 or phoff + phsize * phnum > len(data):
        raise ValueError("invalid program header table")
    executable_entry = False
    for i in range(phnum):
        ptype, flags, offset, vaddr, _paddr, filesz, memsz, align = struct.unpack_from(
            "<IIQQQQQQ", data, phoff + i * phsize)
        if ptype in (2, 3) or (ptype == 0x6474e551 and flags & 1):
            raise ValueError("dynamic loading and executable stack are outside this SDK")
        if ptype != 1:
            continue
        if (filesz > memsz or offset + filesz > len(data) or
                vaddr + memsz > 2**64 or (flags & 3) == 3 or
                align not in (0, 1) and (align & (align - 1) or vaddr % align != offset % align)):
            raise ValueError("invalid or writable executable segment")
        if flags & 1 and vaddr <= entry < vaddr + filesz:
            executable_entry = True
    if not executable_entry:
        raise ValueError("entry is not backed by executable payload bytes")
    return entry


def manifest_for(metadata, binary):
    entry = elf_entry(binary)
    return {"format": FORMAT, "project": project_fields(metadata),
            "arch": ARCH, "interface": INTERFACE,
            "payload": {"name": PAYLOAD, "kind": "elf64-exec",
                        "entry": entry, "bytes": len(binary),
                        "sha256": hashlib.sha256(binary).hexdigest()}}


def encode(value):
    return (json.dumps(value, sort_keys=True, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def write_package(path, metadata, binary):
    raw = encode(manifest_for(metadata, binary))
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, scratch = tempfile.mkstemp(dir=path.parent, suffix=".tmp")
    os.close(fd)
    temporary = Path(scratch)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED) as archive:
            for name, contents in ((MANIFEST, raw), (PAYLOAD, binary)):
                entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                entry.create_system = 3
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, contents)
        verify(temporary)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def verify(path):
    with path.open("rb") as stream:
        if not 1 <= os.fstat(stream.fileno()).st_size <= MAX_PACKAGE:
            raise ValueError("package size out of bounds")
        with zipfile.ZipFile(stream) as archive:
            entries = archive.infolist()
            if [e.filename for e in entries] != [MANIFEST, PAYLOAD]:
                raise ValueError("expected exactly manifest.json and payload.elf in that order")
            for e, maximum in zip(entries, (MAX_MANIFEST, MAX_PAYLOAD)):
                if (e.is_dir() or e.flag_bits & 1 or e.compress_type != zipfile.ZIP_STORED or
                        e.compress_size != e.file_size or not 1 <= e.file_size <= maximum or
                        (e.external_attr >> 16) & 0o170000 not in (0, 0o100000)):
                    raise ValueError("invalid ZIP member")
            raw = archive.read(entries[0])
            binary = archive.read(entries[1])
    metadata = strict_json(raw)
    if not isinstance(metadata, dict):
        raise ValueError("manifest must be an object")
    expected = manifest_for(metadata.get("project"), binary)
    if encode(metadata) != encode(expected):
        raise ValueError("manifest, architecture, interface or payload integrity mismatch")
    return expected


def tool(provided, variable, candidates):
    choice = provided or os.environ.get(variable)
    for name in ((choice,) if choice else candidates):
        found = shutil.which(name)
        if found:
            return found
    raise ValueError(f"missing {variable}; provide a path or put {candidates[0]} on PATH")


def build(args):
    project = args.project.resolve(strict=True)
    application = project / "app.json"
    with application.open("rb") as source:
        raw = source.read(MAX_MANIFEST + 1)
    if len(raw) > MAX_MANIFEST:
        raise ValueError("app.json size out of bounds")
    metadata = project_fields(strict_json(raw))
    output = args.output
    if output.suffix.lower() != ".tea":
        raise ValueError("output file must use .tea")
    source = (project / "main.c").resolve(strict=True)
    clang = tool(args.clang, "CLANG", ("clang", "clang-19", "clang-18"))
    linker = tool(args.lld, "LD_LLD", ("ld.lld", "ld.lld-19", "ld.lld-18"))
    with tempfile.TemporaryDirectory(prefix="styleos-sdk-") as work:
        obj = Path(work) / "app.o"
        exe = Path(work) / PAYLOAD
        subprocess.run([clang, "--target=x86_64-none-elf", "-std=c11", "-O2",
                        "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
                        "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
                        "-Wall", "-Wextra", "-Werror", "-I", str(SDK / "include"),
                        "-c", str(source), "-o", str(obj)], check=True)
        subprocess.run([linker, "--build-id=none", "-z", "noexecstack",
                        "-e", ENTRY, "-o", str(exe), str(obj)], check=True)
        with exe.open("rb") as payload:
            write_package(output, metadata, payload.read(MAX_PAYLOAD + 1))
    result = verify(output)
    print(f"BUILD PASS: {output.resolve()}")
    print(f"payload sha256: {result['payload']['sha256']}")


def new(args):
    meta = project_fields(dict(id=args.id, name=args.name, version="0.1.0"))
    template = (SDK / "templates" / "text-app" / "main.c").read_bytes()
    args.project.mkdir(parents=True, exist_ok=False)
    (args.project / "app.json").write_bytes(encode(meta))
    (args.project / "main.c").write_bytes(template)
    print(f"PROJECT CREATED: {args.project.resolve()}")


def interactive(action):
    if action == "new":
        name = input("Display name: ").strip()
        ident = input("App ID (example.my-app): ").strip()
        metadata = project_fields(dict(id=ident, name=name, version="0.1.0"))
        project = Path.home() / "StyleOSProjects" / metadata["id"]
        new(argparse.Namespace(project=project, id=ident, name=name))
        print("Edit main.c, then run Build project with that folder.")
    else:
        text = input("Project folder (where app.json and main.c are): ").strip()
        if len(text) >= 2 and text[0] == text[-1] == '"':
            text = text[1:-1]
        if not text:
            raise ValueError("choose a project folder")
        project = Path(text).expanduser()
        output = project / "build" / "application.tea"
        build(argparse.Namespace(project=project, output=output, clang=None, lld=None))


def main():
    # User-defined names/paths need not be representable in a redirected Windows console.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("new", help="make a fresh example project")
    create.add_argument("project", type=Path)
    create.add_argument("--id", required=True)
    create.add_argument("--name", required=True)
    b = sub.add_parser("build", help="compile and create experimental .tea")
    b.add_argument("project", type=Path)
    b.add_argument("--output", type=Path, required=True)
    b.add_argument("--clang")
    b.add_argument("--lld")
    check = sub.add_parser("verify", help="inspect without installing or executing")
    check.add_argument("package", type=Path)
    guided = sub.add_parser("interactive", help="guided project creation or build")
    guided.add_argument("action", choices=("new", "build"))
    args = parser.parse_args()
    try:
        if args.command == "new":
            new(args)
        elif args.command == "build":
            build(args)
        elif args.command == "interactive":
            interactive(args.action)
        else:
            result = verify(args.package)
            print("VERIFY PASS")
            print(encode(result).decode("utf-8"), end="")
    except (OSError, EOFError, ValueError, RuntimeError, subprocess.CalledProcessError,
            zipfile.BadZipFile, struct.error) as exc:
        print(f"SDK FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
