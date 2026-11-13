#!/usr/bin/env python3
"""Prove complete boot-pool intervals from ELF/PE images, using only stdlib.

PE addresses are RVAs, never runtime addresses. The caller must add the actual
loaded-image base reported by the platform. ARM addresses use the supported
identity-mapped QEMU Image profile. A linker map is evidence associated with a
PE, not an independently authenticated description of an arbitrary PE.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

try:
    from .fault_site import _range, _unpack, _string, _read_file
except ImportError:  # Direct tools/pool_site.py and tools/build.py invocation.
    from fault_site import _range, _unpack, _string, _read_file

PAGE = 4096
POOL_BYTES = 8 * PAGE
METADATA_BYTES = PAGE
ARM_BASE = 0x40080000
U64_LIMIT = 1 << 64
U32_LIMIT = 1 << 32
POOL_SYMBOL = 'styleos_bootpool_storage'
METADATA_SYMBOL = 'styleos_bootpool_metadata'
OBJECTS = {POOL_SYMBOL: POOL_BYTES, METADATA_SYMBOL: METADATA_BYTES}


def _interval(start, size, description, limit=U64_LIMIT):
    if start < 0 or size <= 0 or start >= limit or size > limit - start:
        raise ValueError(f'invalid or overflowing {description} interval')
    return start, start + size


def _inside(interval, outer):
    return outer[0] <= interval[0] < interval[1] <= outer[1]


def _intersects(left, right):
    return left[0] < right[1] and right[0] < left[1]


def _layout(objects):
    pool, metadata = objects[POOL_SYMBOL], objects[METADATA_SYMBOL]
    for name, bounds in (('pool', pool), ('metadata', metadata)):
        if bounds[0] % PAGE or bounds[1] % PAGE:
            raise ValueError(f'{name} is not page aligned')
    if pool[0] < metadata[1] and metadata[0] < pool[1]:
        raise ValueError('pool and metadata overlap')
    return {'pool_start': pool[0], 'pool_end': pool[1],
            'metadata_start': metadata[0], 'metadata_end': metadata[1],
            'pool_bytes': POOL_BYTES, 'metadata_bytes': METADATA_BYTES,
            'page_size': PAGE}


def _elf_pool(data, raw):
    if _range(data, 0, 16, 'ELF identification')[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected ELF64 little-endian version 1')
    (kind, machine, version, entry, phoff, shoff, _flags, ehsize, phsize,
     phnum, shsize, shnum, shstrndx) = _unpack('<HHIQQQIHHHHHH', data, 16, 'ELF header')
    if kind != 2 or machine != 183 or version != 1 or ehsize != 64:
        raise ValueError('expected an AArch64 executable ELF')
    if not phnum or phnum == 0xffff or phsize != 56:
        raise ValueError('unsupported ELF program header layout')
    if not shnum or shsize != 64 or not 0 < shstrndx < shnum:
        raise ValueError('unsupported ELF section header layout')
    _range(data, phoff, phnum * phsize, 'ELF program headers')
    _range(data, shoff, shnum * shsize, 'ELF section headers')
    sections = [_unpack('<IIQQQQIIQQ', data, shoff + n * shsize, 'ELF section')
                for n in range(shnum)]
    for section in sections:
        if section[3] + section[5] > U64_LIMIT:
            raise ValueError('ELF section address overflows')
        if section[1] != 8:
            _range(data, section[4], section[5], 'ELF section contents')
    names_section = sections[shstrndx]
    if names_section[1] != 3:
        raise ValueError('ELF section names are not a string table')
    names = _range(data, names_section[4], names_section[5], 'ELF section names')
    symtabs = [s for s in sections if _string(names, s[0], 'section') == b'.symtab']
    if len(symtabs) != 1 or symtabs[0][1] != 2:
        raise ValueError('expected exactly one ELF .symtab')
    symtab = symtabs[0]
    if symtab[9] != 24 or symtab[5] % 24 or not 0 < symtab[6] < shnum:
        raise ValueError('invalid ELF symbol table layout')
    strings_section = sections[symtab[6]]
    if strings_section[1] != 3:
        raise ValueError('ELF symbols do not link to a string table')
    strings = _range(data, strings_section[4], strings_section[5], 'ELF symbol names')
    wanted = {name: [] for name in (*OBJECTS, 'image_start', '__image_end',
                                    '__bss_start', '__bss_end')}
    for offset in range(symtab[4], symtab[4] + symtab[5], 24):
        name, info, other, index, value, size = _unpack('<IBBHQQ', data, offset, 'ELF symbol')
        name = _string(strings, name, 'symbol')
        for key in wanted:
            if name == key.encode():
                wanted[key].append((info, other, index, value, size))
    symbols = {}
    for name, matches in wanted.items():
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate ELF symbol {name}')
        info, other, index, value, size = matches[0]
        expected_type = (1,) if name in OBJECTS else (0, 2)
        if (info >> 4 != 1 or info & 15 not in expected_type or other & ~3
                or not 0 < index < shnum):
            raise ValueError(f'suspicious ELF symbol {name}')
        symbols[name] = (index, value, size)
    loads = []
    for n in range(phnum):
        p = _unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF program header')
        if p[0] == 1:
            if (p[5] > p[6] or p[3] + p[6] > U64_LIMIT
                    or p[4] + p[6] > U64_LIMIT):
                raise ValueError('invalid ELF PT_LOAD bounds')
            _range(data, p[2], p[5], 'ELF PT_LOAD contents')
            loads.append(p)
    image_start = symbols['image_start'][1]
    image_end = symbols['__image_end'][1]
    bss_start = symbols['__bss_start'][1]
    bss_end = symbols['__bss_end'][1]
    if image_start != ARM_BASE or entry != image_start or image_end <= image_start:
        raise ValueError('unexpected AArch64 image interval or entry')
    image = _interval(image_start, image_end - image_start, 'ARM image')
    if (bss_start % 8 or bss_end % 8 or not _inside((bss_start, bss_end), image)):
        raise ValueError('invalid BSS clear bounds')
    # Prove the real Image entry/header is file-backed executable code.
    index, _, _ = symbols['image_start']
    section = sections[index]
    header_bounds = _interval(image_start, 64, 'ARM Image header')
    if (section[1] != 1 or section[2] & 6 != 6
            or not _inside(header_bounds, (section[3], section[3] + section[5]))):
        raise ValueError('ARM Image header is not executable section data')
    header_offset = section[4] + image_start - section[3]
    mappings = [p for p in loads if p[1] & 1 and
                _inside(header_bounds, (p[3], p[3] + p[5]))]
    if (len(mappings) != 1 or mappings[0][3] != mappings[0][4]
            or mappings[0][2] + image_start - mappings[0][3] != header_offset):
        raise ValueError('ARM Image header and PT_LOAD disagree')
    objects, section_names = {}, {}
    for name, size in OBJECTS.items():
        index, address, declared_size = symbols[name]
        section = sections[index]
        bounds = _interval(address, size, name)
        if declared_size != size:
            raise ValueError(f'{name} has unexpected ELF symbol size')
        if (section[1] != 8 or section[2] & 3 != 3
                or not _inside(bounds, (section[3], section[3] + section[5]))):
            raise ValueError(f'{name} is not wholly in writable allocated NOBITS')
        mappings = [p for p in loads if
                    _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(mappings) != 1 or not mappings[0][1] & 2
                or not _inside(bounds, (mappings[0][3], mappings[0][3] + mappings[0][6]))
                or mappings[0][3] != mappings[0][4]):
            raise ValueError(f'{name} has no unique identity-mapped writable PT_LOAD')
        if not _inside(bounds, (bss_start, bss_end)):
            raise ValueError(f'{name} is outside BSS clear bounds')
        objects[name] = bounds
        section_names[name] = _string(names, section[0], 'section').decode('ascii')
    result = _layout(objects)
    header = _range(raw, 0, 64, 'raw ARM Image header')
    if header != _range(data, header_offset, 64, 'ELF ARM Image header'):
        raise ValueError('raw ARM Image header disagrees with ELF')
    if (_unpack('<Q', header, 8, 'ARM text_offset')[0] != 0x80000
            or _unpack('<Q', header, 16, 'ARM image_size')[0] != image_end - image_start
            or _unpack('<I', header, 56, 'ARM Image magic')[0] != 0x644d5241):
        raise ValueError('raw ARM Image size, offset or magic is invalid')
    if len(raw) > image_end - image_start:
        raise ValueError('raw ARM Image exceeds declared image interval')
    # Compare every allocated file-backed section, not just its first address.
    for s in sections:
        if s[1] != 8 and s[2] & 2 and s[5]:
            bounds = _interval(s[3], s[5], 'allocated ELF section')
            if not _inside(bounds, image):
                raise ValueError('allocated ELF section is outside Image')
            if _range(raw, s[3] - image_start, s[5], 'raw Image section') != \
                    _range(data, s[4], s[5], 'ELF allocated section'):
                raise ValueError('raw ARM Image section disagrees with ELF')
    result.update(arch='aarch64', address_space='identity-image-address',
                  image_start=image_start, image_end=image_end,
                  bss_start=bss_start, bss_end=bss_end,
                  image_size=image_end - image_start, sections=section_names)
    return result


def _pe_pool(data, map_bytes):
    if _range(data, 0, 64, 'DOS header')[:2] != b'MZ':
        raise ValueError('expected a PE DOS header')
    pe = _unpack('<I', data, 60, 'PE header offset')[0]
    if pe < 64 or _range(data, pe, 4, 'PE signature') != b'PE\0\0':
        raise ValueError('invalid PE signature or header offset')
    machine, count, _time, _symbols, _number, opt_size, _flags = _unpack(
        '<HHIIIHH', data, pe + 4, 'COFF header')
    if machine != 0x8664 or not 0 < count <= 96 or opt_size < 112:
        raise ValueError('expected AMD64 PE32+ section layout')
    opt = _range(data, pe + 24, opt_size, 'PE optional header')
    if _unpack('<H', opt, 0, 'PE optional magic')[0] != 0x20b:
        raise ValueError('expected PE32+ optional header')
    base = _unpack('<Q', opt, 24, 'PE ImageBase')[0]
    image_size, headers = _unpack('<II', opt, 56, 'PE image size')
    section_offset = pe + 24 + opt_size
    _range(data, section_offset, count * 40, 'PE sections')
    if (not image_size or base + image_size > U64_LIMIT or headers > len(data)
            or headers < section_offset + count * 40):
        raise ValueError('invalid PE image or header bounds')
    sections = []
    for n in range(count):
        s = _unpack('<8sIIIIIIHHI', data, section_offset + n * 40, 'PE section')
        _name, virtual_size, va, raw_size, raw_offset = s[:5]
        extent = virtual_size or raw_size
        if va + extent > min(U32_LIMIT, image_size):
            raise ValueError('PE section lies outside SizeOfImage')
        if raw_size:
            if raw_offset < headers:
                raise ValueError('PE section overlaps headers')
            _range(data, raw_offset, raw_size, 'PE section contents')
        sections.append(s)
    if b'\0' in map_bytes:
        raise ValueError('linker map contains NUL bytes')
    lines = map_bytes.decode('utf-8').splitlines()
    preferred_rows = [line for line in lines if 'Preferred load address is' in line]
    if preferred_rows:
        m = re.fullmatch(r'\s*Preferred load address is ([0-9a-fA-F]{1,16})\s*', preferred_rows[0])
        if len(preferred_rows) != 1 or m is None or int(m[1], 16) != base:
            raise ValueError('linker map preferred load address disagrees with PE ImageBase')
    headings = [n for n, line in enumerate(lines) if re.fullmatch(
        r'\s*Address\s+Publics by Value\s+Rva\+Base\s+Lib:Object\s*', line)]
    if len(headings) != 1:
        raise ValueError('missing or duplicate linker map Rva+Base heading')
    objects, section_names = {}, {}
    for name, size in OBJECTS.items():
        rows = [(n, line.split()) for n, line in enumerate(lines) if name in line.split()]
        if len(rows) != 1:
            raise ValueError(f'missing or duplicate linker map symbol {name}')
        number, fields = rows[0]
        if (number <= headings[0] or len(fields) < 4 or fields[1] != name
                or not re.fullmatch(r'[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,16}', fields[0])
                or not re.fullmatch(r'[0-9a-fA-F]{8,16}', fields[2])):
            raise ValueError(f'suspicious linker map symbol row {name}')
        segment, relative = (int(v, 16) for v in fields[0].split(':'))
        address = int(fields[2], 16)
        if address < base:
            raise ValueError('map Rva+Base is below PE preferred ImageBase')
        bounds = _interval(address - base, size, name, U32_LIMIT)
        if bounds[1] > image_size:
            raise ValueError(f'{name} extends beyond PE SizeOfImage')
        candidates = [(n, s) for n, s in enumerate(sections, 1)
                      if _intersects(bounds, (s[2], s[2] + (s[1] or s[3])))]
        if (len(candidates) != 1
                or not _inside(bounds, (candidates[0][1][2], candidates[0][1][2] +
                                        (candidates[0][1][1] or candidates[0][1][3])))):
            raise ValueError(f'{name} has no unique complete PE section')
        n, s = candidates[0]
        if (segment != n or relative != bounds[0] - s[2]
                or not s[9] & 0x80000000 or not s[9] & 0xc0):
            raise ValueError(f'{name} is not in matching writable PE data')
        # A BSS object may share .data with initialized bytes. Its own raw
        # prefix (if any) must be zero; the loader zero-fills the virtual tail.
        present = max(0, min(size, s[3] - relative))
        if present and any(_range(data, s[4] + relative, present, 'PE pool initial data')):
            raise ValueError(f'{name} has nonzero initialized bytes')
        objects[name] = bounds
        section_names[name] = s[0].split(b'\0', 1)[0].decode('ascii')
    result = _layout(objects)
    result.update(arch='x86_64', address_space='image-rva',
                  pool_rva_start=result['pool_start'], pool_rva_end=result['pool_end'],
                  metadata_rva_start=result['metadata_start'],
                  metadata_rva_end=result['metadata_end'],
                  preferred_image_base=base, image_size=image_size,
                  image_start=0, image_end=image_size, sections=section_names)
    return result


def describe_pool(arch, binary, map_path=None, raw_path=None):
    """Return verified half-open intervals and input hashes, or raise ValueError.

    ARM requires the linked ELF and its raw Image. PE requires lld-link /map
    output. These static proofs must be combined with live platform evidence;
    no arbitrary C pointer is interpreted as a physical address here.
    """
    if arch not in ('aarch64', 'x86_64'):
        raise ValueError(f'unsupported pool architecture: {arch}')
    try:
        data = _read_file(binary, 'binary')
        if arch == 'aarch64':
            if raw_path is None:
                raise ValueError('AArch64 pool proof requires its raw Image')
            raw = _read_file(raw_path, 'raw Image')
            result = _elf_pool(data, raw)
            result['raw_sha256'] = hashlib.sha256(raw).hexdigest()
        else:
            if map_path is None:
                raise ValueError('x86_64 pool proof requires its linker map')
            map_bytes = _read_file(map_path, 'linker map')
            result = _pe_pool(data, map_bytes)
            result['map_sha256'] = hashlib.sha256(map_bytes).hexdigest()
        result['binary_sha256'] = hashlib.sha256(data).hexdigest()
        return result
    except (OSError, UnicodeError, struct.error) as error:
        raise ValueError(f'cannot describe pool: {error}') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=('aarch64', 'x86_64'), required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--map', dest='map_path', type=Path)
    parser.add_argument('--raw', dest='raw_path', type=Path)
    args = parser.parse_args()
    try:
        result = describe_pool(args.arch, args.binary, args.map_path, args.raw_path)
    except ValueError as error:
        parser.exit(1, f'pool-site error: {error}\n')
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
