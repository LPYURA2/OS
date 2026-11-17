#!/usr/bin/env python3
"""Build the independent T-P1.8B AArch64 fixture with ordinary Clang and LLD.

No kernel build, LLVM objdump/objcopy, package install, or network is needed.
The linked ELF is provenance, and only its 28 code bytes enter app.bin.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys
import zlib

try:
    from .disk_app_site import build_app_metadata
except ImportError:
    from disk_app_site import build_app_metadata

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {'a': (0x1818a001, 41), 'b': (0x1818b002, 73)}
LABELS = dict(disk_app_entry=0, disk_app_fault=8, disk_app_normal=12,
              disk_app_exit=24, disk_app_end=28)


def _tool(variable, names):
    candidates = [os.environ[variable]] if os.environ.get(variable) else names
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError(f'missing {variable}: install {" / ".join(names)}')


def _sha(data):
    return hashlib.sha256(data).hexdigest()


def _elf_labels(data, code):
    """Check ELF executable bytes and resolve real linked labels, not ASM text."""
    if len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected little-endian ELF64')
    h = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    if h[:4] != (2, 183, 1, 0) or h[7] != 64 or h[8] != 56 or h[10] != 64:
        raise ValueError('unsupported AArch64 ELF header')

    def block(start, size):
        if start < 0 or size < 0 or start > len(data) or size > len(data)-start:
            raise ValueError('ELF field outside file')
        return data[start:start+size]

    def string(table, offset):
        if not 0 <= offset < len(table) or (end := table.find(b'\0', offset)) < 0:
            raise ValueError('bad ELF string')
        return table[offset:end].decode('ascii')

    block(h[4], h[8]*h[9]); block(h[5], h[10]*h[11])
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, h[5]+n*64)
                for n in range(h[11])]
    loads = [struct.unpack_from('<IIQQQQQQ', data, h[4]+n*56) for n in range(h[9])]
    loads = [p for p in loads if p[0] == 1]
    if len(loads) != 1:
        raise ValueError('payload must have one PT_LOAD')
    p = loads[0]
    if p[1] != 5 or p[3:7] != (0, 0, len(code), len(code)) or block(p[2], p[5]) != code:
        raise ValueError('ELF executable PT_LOAD differs from complete raw payload')
    symtabs = [s for s in sections if s[1] == 2]
    if len(symtabs) != 1:
        raise ValueError('payload needs one symbol table')
    table = symtabs[0]
    if table[9] != 24 or table[5] % 24 or not 0 <= table[6] < len(sections):
        raise ValueError('invalid ELF symbols')
    strings = sections[table[6]]; names = block(strings[4], strings[5])
    block(table[4], table[5]); symbols = {}
    for off in range(table[4], table[4]+table[5], 24):
        name, info, _other, index, value, _size = struct.unpack_from('<IBBHQQ', data, off)
        name = string(names, name)
        if name not in LABELS:
            continue
        if name in symbols or info >> 4 != 1 or not 0 < index < len(sections):
            raise ValueError('missing or ambiguous executable label')
        section = sections[index]
        if section[1] != 1 or section[2] != 6 or section[3] != 0 or section[5] != len(code):
            raise ValueError('payload label is not in the executable section')
        symbols[name] = value
    if symbols != LABELS:
        raise ValueError('linked payload labels do not match the finite contract')
    return symbols


def build(output, marker, exit_code, *, source=None, linker_script=None):
    if not 0 <= marker <= 0xffffffff or not 0 <= exit_code <= 0xffff:
        raise ValueError('marker must fit u32 and exit code must fit u16')
    source = Path(source) if source else ROOT/'apps/disk_demo.S'
    linker_script = Path(linker_script) if linker_script else ROOT/'apps/disk_demo.ld'
    source = source.absolute(); linker_script = linker_script.absolute()
    for path in (source, linker_script):
        if path.is_symlink() or not stat.S_ISREG(path.stat().st_mode):
            raise ValueError('payload source and linker script must be regular files')
    output = Path(output).absolute()
    if output.is_symlink():
        raise ValueError('payload output must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    paths = {name: output/name for name in
             ('disk_demo.o', 'disk_demo.elf', 'disk_demo.raw', 'disk_demo.map', 'app.bin', 'app.json')}
    if any(path.exists() or path.is_symlink() for path in paths.values()):
        raise ValueError('payload artifacts already exist; use a fresh output directory')
    cc = _tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    ld = _tool('LD_LLD', ['ld.lld', 'ld.lld-19', 'ld.lld-18'])
    commands = [[cc, '--target=aarch64-none-elf', '-c', '-mgeneral-regs-only',
                 f'-DAPP_MARKER={marker}', f'-DAPP_EXIT={exit_code}', str(source),
                 '-o', str(paths['disk_demo.o'])],
                [ld, '-T', str(linker_script), str(paths['disk_demo.o']),
                 '-Map='+str(paths['disk_demo.map']), '-o', str(paths['disk_demo.elf'])],
                [ld, '-T', str(linker_script), '--oformat=binary',
                 str(paths['disk_demo.o']), '-o', str(paths['disk_demo.raw'])]]
    for command in commands:
        print('+', ' '.join(command), flush=True)
        subprocess.run(command, check=True)
    raw = paths['disk_demo.raw'].read_bytes()
    expected = struct.pack('<7I', 0xb5000060, 0xd2a24021, 0xf9400022,
                           0xd2800013 | ((marker & 0xffff) << 5),
                           0xf2a00013 | ((marker >> 16) << 5),
                           0xd2800000 | (exit_code << 5), 0xd4000041)
    if raw != expected:
        raise ValueError('assembled executable differs from the seven-instruction grammar')
    elf = paths['disk_demo.elf'].read_bytes()
    labels = _elf_labels(elf, raw)
    header = struct.pack('<8sIHHIIII', b'STYAPP1\0', 1, 183, 32, len(raw), 0, 0, 0)
    container = bytearray(header+raw)
    checksum = zlib.crc32(container)
    struct.pack_into('<I', container, 28, checksum)
    with paths['app.bin'].open('xb') as stream:
        stream.write(container)
    metadata = dict(format='StyleOS T-P1.8B fixture v1', isa='aarch64',
        header_length=32, code_length=len(raw), file_length=len(container),
        entry_offset=labels['disk_app_entry'], fault_offset=labels['disk_app_fault'],
        normal_offset=labels['disk_app_normal'], exit_offset=labels['disk_app_exit'],
        exit_resume_offset=labels['disk_app_exit']+4, marker=marker,
        exit_code=exit_code, checksum=checksum, labels=labels,
        code_hex=raw.hex(), container_hex=container.hex(), file_hex=container.hex(),
        code_sha256=_sha(raw), raw_sha256=_sha(raw), container_sha256=_sha(container),
        file_sha256=_sha(container), payload_crc32=zlib.crc32(container.ljust(1024, b'\0')),
        elf_sha256=_sha(elf), source_sha256=_sha(source.read_bytes()),
        linker_sha256=_sha(linker_script.read_bytes()),
        map_sha256=_sha(paths['disk_demo.map'].read_bytes()),
        source=str(source), linker_script=str(linker_script), commands=commands,
        artifacts={key: str(value) for key, value in paths.items()},
        compiler_version=subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
        linker_version=subprocess.check_output([ld, '--version'], text=True).splitlines()[0],
        instruction_proof='all 28 executable bytes match the closed seven-instruction integer grammar')
    independent = build_app_metadata(paths['disk_demo.elf'], source, paths['disk_demo.raw'])
    if independent['file_hex'] != container.hex():
        raise ValueError('independent ELF/container proof disagrees with builder')
    metadata.update(independent)
    with paths['app.json'].open('x') as stream:
        json.dump(metadata, stream, indent=2, sort_keys=True); stream.write('\n')
    return metadata


def build_variant(output, variant):
    try:
        marker, exit_code = VARIANTS[variant.lower()]
    except (KeyError, AttributeError) as error:
        raise ValueError('variant must be a or b') from error
    return build(output, marker, exit_code)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--marker', type=lambda s: int(s, 0), default=VARIANTS['a'][0])
    parser.add_argument('--exit-code', type=lambda s: int(s, 0), default=VARIANTS['a'][1])
    parser.add_argument('--source', type=Path)
    parser.add_argument('--linker-script', type=Path)
    args = parser.parse_args()
    try:
        info = build(args.output, args.marker, args.exit_code,
                     source=args.source, linker_script=args.linker_script)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'disk application build: {error}\n')
    print(f'Built {info["file_length"]}-byte app.bin; SHA256 {info["file_sha256"]}')


if __name__ == '__main__':
    main()
