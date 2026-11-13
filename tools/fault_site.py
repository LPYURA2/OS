#!/usr/bin/env python3
"""Resolve a managed test fault from the built binary, without external tools.

`offset` always means a raw byte offset in the supplied file. AArch64 `pc` is
the linked ELF address, while x86 `rva` must be added to the image's actual UART
load address by the caller. A PE preferred ImageBase is never a runtime base.
`instruction_hex` is the literal file-byte sequence, in file order.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import stat
import struct


SYMBOL = 'styleos_test_fault'
ARM_IMAGE_BASE = 0x40080000
ARM_INSTRUCTION = bytes.fromhex('002420d4')  # BRK #0x120, word 0xd4202400.
X86_INSTRUCTION = bytes.fromhex('0f0b')       # UD2.
U64_LIMIT = 1 << 64
U32_LIMIT = 1 << 32


def _range(data, offset, size, description):
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ValueError(f'{description} is outside the file')
    return data[offset:offset + size]


def _unpack(fmt, data, offset, description):
    size = struct.calcsize(fmt)
    _range(data, offset, size, description)
    return struct.unpack_from(fmt, data, offset)


def _string(table, offset, description):
    if not 0 <= offset < len(table):
        raise ValueError(f'invalid {description} string offset')
    end = table.find(b'\0', offset)
    if end < 0:
        raise ValueError(f'unterminated {description} string')
    return table[offset:end]


def _read_file(path, description):
    path = Path(path)
    if not stat.S_ISREG(path.stat().st_mode):
        raise ValueError(f'{description} must be a regular file')
    return path.read_bytes()


def _elf_site(data):
    if _range(data, 0, 16, 'ELF identification')[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected ELF64 little-endian version 1')
    header = _unpack('<HHIQQQIHHHHHH', data, 16, 'ELF header')
    (kind, machine, version, entry, phoff, shoff, _flags, ehsize, phsize,
     phnum, shsize, shnum, shstrndx) = header
    if kind != 2 or machine != 183 or version != 1 or ehsize != 64:
        raise ValueError('expected an AArch64 executable ELF')
    if not phnum or phnum == 0xffff or phsize != 56:
        raise ValueError('unsupported ELF program header layout')
    if not shnum or shsize != 64 or not 0 < shstrndx < shnum:
        raise ValueError('unsupported ELF section header layout')
    _range(data, phoff, phnum * phsize, 'ELF program headers')
    _range(data, shoff, shnum * shsize, 'ELF section headers')
    sections = [_unpack('<IIQQQQIIQQ', data, shoff + n * shsize,
                        'ELF section') for n in range(shnum)]
    for section in sections:
        if section[3] + section[5] > U64_LIMIT:
            raise ValueError('ELF section address overflows')
        if section[1] != 8:  # SHT_NOBITS has no raw bytes.
            _range(data, section[4], section[5], 'ELF section contents')
    names_section = sections[shstrndx]
    if names_section[1] != 3:
        raise ValueError('ELF section names are not a string table')
    names = _range(data, names_section[4], names_section[5], 'ELF section names')
    symtabs = [section for section in sections
               if _string(names, section[0], 'section') == b'.symtab']
    if len(symtabs) != 1 or symtabs[0][1] != 2:
        raise ValueError('expected exactly one ELF .symtab')
    symtab = symtabs[0]
    if symtab[9] != 24 or symtab[5] % 24 or not 0 < symtab[6] < shnum:
        raise ValueError('invalid ELF symbol table layout')
    strings_section = sections[symtab[6]]
    if strings_section[1] != 3:
        raise ValueError('ELF symbols do not link to a string table')
    strings = _range(data, strings_section[4], strings_section[5], 'ELF symbols names')
    wanted = {SYMBOL.encode(): [], b'image_start': []}
    for offset in range(symtab[4], symtab[4] + symtab[5], 24):
        name, info, other, index, value, size = _unpack(
            '<IBBHQQ', data, offset, 'ELF symbol')
        name = _string(strings, name, 'symbol')
        if name in wanted:
            wanted[name].append((info, other, index, value, size))
    for name, matches in wanted.items():
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate ELF symbol {name.decode()}')
        info, other, index, _value, _size = matches[0]
        if (info >> 4 != 1 or info & 15 not in (0, 2)
                or other & ~3 or not 0 < index < shnum):
            raise ValueError(f'suspicious ELF symbol {name.decode()}')

    loads = []
    for n in range(phnum):
        p = _unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF program header')
        if p[0] == 1:
            if p[5] > p[6] or p[3] + p[6] > U64_LIMIT:
                raise ValueError('invalid ELF PT_LOAD bounds')
            _range(data, p[2], p[5], 'ELF PT_LOAD contents')
            loads.append(p)

    def resolve(match, width):
        _info, _other, index, address, declared_size = match
        section = sections[index]
        if (section[1] != 1 or section[2] & 6 != 6
                or address < section[3]
                or width > section[5] - (address - section[3])
                or declared_size and not width <= declared_size <=
                section[5] - (address - section[3])):
            raise ValueError('ELF symbol is not backed by executable section bytes')
        offset = section[4] + address - section[3]
        mappings = [p for p in loads if p[1] & 1 and address >= p[3]
                    and address - p[3] <= p[5] - width]
        if (len(mappings) != 1 or mappings[0][4] != mappings[0][3]
                or mappings[0][2] + address - mappings[0][3] != offset):
            raise ValueError('ELF section and executable PT_LOAD disagree')
        return address, offset

    image_base, _image_offset = resolve(wanted[b'image_start'][0], 4)
    if image_base != ARM_IMAGE_BASE or entry != image_base:
        raise ValueError('unexpected AArch64 image_start or ELF entry address')
    pc, offset = resolve(wanted[SYMBOL.encode()][0], len(ARM_INSTRUCTION))
    if pc < image_base or pc % 4:
        raise ValueError('invalid AArch64 fault PC alignment or image offset')
    instruction = _range(data, offset, len(ARM_INSTRUCTION), 'AArch64 instruction')
    if instruction != ARM_INSTRUCTION:
        raise ValueError('fault symbol does not point to BRK #0x120')
    return {'arch': 'aarch64', 'symbol': SYMBOL, 'pc': pc, 'offset': offset,
            'image_base': image_base, 'image_offset': pc - image_base,
            'instruction_hex': instruction.hex(),
            'instruction_word': int.from_bytes(instruction, 'little')}


def _pe_site(data, map_bytes):
    if _range(data, 0, 64, 'DOS header')[:2] != b'MZ':
        raise ValueError('expected a PE DOS header')
    pe_offset = _unpack('<I', data, 60, 'PE header offset')[0]
    if pe_offset < 64 or _range(data, pe_offset, 4, 'PE signature') != b'PE\0\0':
        raise ValueError('invalid PE signature or header offset')
    machine, count, _time, _symbols, _number, optional_size, _flags = _unpack(
        '<HHIIIHH', data, pe_offset + 4, 'COFF header')
    if machine != 0x8664 or not 0 < count <= 96 or optional_size < 112:
        raise ValueError('expected AMD64 PE32+ section layout')
    optional_offset = pe_offset + 24
    optional = _range(data, optional_offset, optional_size, 'PE optional header')
    if _unpack('<H', optional, 0, 'PE optional magic')[0] != 0x20b:
        raise ValueError('expected PE32+ optional header')
    preferred_base = _unpack('<Q', optional, 24, 'PE ImageBase')[0]
    image_size, headers_size = _unpack('<II', optional, 56, 'PE image size')
    section_offset = optional_offset + optional_size
    _range(data, section_offset, count * 40, 'PE sections')
    if (not image_size or preferred_base + image_size > U64_LIMIT
            or headers_size > len(data)
            or headers_size < section_offset + count * 40):
        raise ValueError('invalid PE image or header bounds')
    sections = []
    for n in range(count):
        section = _unpack('<8sIIIIIIHHI', data, section_offset + n * 40, 'PE section')
        _name, virtual_size, va, raw_size, raw_offset = section[:5]
        extent = virtual_size or raw_size
        if va + extent > min(U32_LIMIT, image_size):
            raise ValueError('PE section lies outside SizeOfImage')
        if raw_size:
            if raw_offset < headers_size:
                raise ValueError('PE section overlaps headers')
            _range(data, raw_offset, raw_size, 'PE section contents')
        sections.append(section)

    if b'\0' in map_bytes:
        raise ValueError('linker map contains NUL bytes')
    lines = map_bytes.decode('utf-8').splitlines()
    preferred_rows = [line for line in lines if 'Preferred load address is' in line]
    if preferred_rows:
        preferred_match = re.fullmatch(
            r'\s*Preferred load address is ([0-9a-fA-F]{1,16})\s*', preferred_rows[0])
        if (len(preferred_rows) != 1 or preferred_match is None
                or int(preferred_match[1], 16) != preferred_base):
            raise ValueError('linker map preferred load address disagrees with PE ImageBase')
    headings = [n for n, line in enumerate(lines) if re.fullmatch(
        r'\s*Address\s+Publics by Value\s+Rva\+Base\s+Lib:Object\s*', line)]
    if len(headings) != 1:
        raise ValueError('missing or duplicate linker map Rva+Base heading')
    matches = [(n, line.split()) for n, line in enumerate(lines)
               if SYMBOL in line.split()]
    if len(matches) != 1:
        raise ValueError('missing or duplicate linker map fault symbol')
    line_number, fields = matches[0]
    if (line_number <= headings[0] or len(fields) < 4 or fields[1] != SYMBOL
            or not re.fullmatch(r'[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,16}', fields[0])
            or not re.fullmatch(r'[0-9a-fA-F]{8,16}', fields[2])):
        raise ValueError('suspicious linker map fault symbol row')
    segment, segment_offset = (int(value, 16) for value in fields[0].split(':'))
    absolute = int(fields[2], 16)
    if absolute < preferred_base or absolute >= U64_LIMIT:
        raise ValueError('map Rva+Base is below PE preferred ImageBase')
    rva = absolute - preferred_base
    if rva >= U32_LIMIT:
        raise ValueError('map fault RVA exceeds PE address range')
    candidates = []
    for n, section in enumerate(sections, 1):
        _name, virtual_size, va, raw_size, raw_offset = section[:5]
        extent = virtual_size or raw_size
        relative = rva - va
        if relative >= 0 and relative + 2 <= extent:
            candidates.append((n, section, relative, raw_offset + relative))
    if len(candidates) != 1:
        raise ValueError('map fault RVA has no unique PE section')
    number, section, relative, offset = candidates[0]
    if (not section[9] & 0x20000000 or relative + 2 > section[3]
            or segment != number or segment_offset != relative):
        raise ValueError('fault symbol is not backed by matching executable PE bytes')
    instruction = _range(data, offset, 2, 'x86 instruction')
    if instruction != X86_INSTRUCTION:
        raise ValueError('fault symbol does not point to UD2')
    return {'arch': 'x86_64', 'symbol': SYMBOL, 'rva': rva,
            'preferred_image_base': preferred_base, 'offset': offset,
            'instruction_hex': instruction.hex()}


def describe_fault(arch, binary_path, map_path=None):
    """Return verified symbol/opcode evidence or raise ValueError.

    Only AArch64 ELF executables and AMD64 PE32+ images are supported. The PE
    sidecar must be the lld-link /map output for that image. It establishes an
    RVA; it cannot establish the firmware-selected runtime image address.
    """
    if arch not in ('aarch64', 'x86_64'):
        raise ValueError(f'unsupported fault-site architecture: {arch}')
    try:
        data = _read_file(binary_path, 'binary')
        if arch == 'aarch64':
            return _elf_site(data)
        if map_path is None:
            raise ValueError('x86_64 fault-site proof requires its linker map')
        return _pe_site(data, _read_file(map_path, 'linker map'))
    except (OSError, UnicodeError, struct.error) as error:
        raise ValueError(f'cannot describe fault site: {error}') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=('aarch64', 'x86_64'), required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--map', dest='map_path', type=Path)
    args = parser.parse_args()
    try:
        result = describe_fault(args.arch, args.binary, args.map_path)
    except ValueError as error:
        parser.exit(1, f'fault-site error: {error}\n')
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
