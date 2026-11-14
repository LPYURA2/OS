#!/usr/bin/env python3
"""Prove the complete image-owned paging reserve and managed read instruction.

PE coordinates are RVAs; a caller must combine them with the actual loaded
ImageBase. ARM coordinates belong only to the fixed MMU-off QEMU Image profile.
The linker map is build-associated evidence, not authentication of foreign PE.
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
except ImportError:
    from fault_site import _range, _unpack, _string, _read_file

PAGE = 4096
ARM_BASE = 0x40080000
U64 = 1 << 64
U32 = 1 << 32
ALIAS = 0x0000400000000000
PROBE = ALIAS + PAGE
INITIAL = bytes.fromhex('f0debc9a78563412') + bytes(PAGE - 8)
OBJECTS = {'tables': ('styleos_paging_tables', 32 * PAGE),
           'control': ('styleos_paging_control', PAGE),
           'data': ('styleos_paging_data', PAGE)}
POOL = {'pool': ('styleos_bootpool_storage', 8 * PAGE),
        'pool_metadata': ('styleos_bootpool_metadata', PAGE)}
FAULT = 'styleos_paging_fault'
INSTRUCTION = {'x86_64': bytes.fromhex('488b01'),
               'aarch64': bytes.fromhex('010040f9')}


def _interval(start, size, what, limit=U64):
    if start < 0 or size <= 0 or start >= limit or size > limit - start:
        raise ValueError(f'invalid or overflowing {what} extent')
    return start, start + size


def _inside(inner, outer):
    return outer[0] <= inner[0] < inner[1] <= outer[1]


def _overlap(a, b):
    return a[0] < b[1] and b[0] < a[1]


def _finish(objects, pool):
    all_objects = list(objects.items()) + list(pool.items())
    for name, item in all_objects:
        if (item['start'] | item['end']) & (PAGE - 1):
            raise ValueError(f'{name} is not page aligned')
    for index, (name, item) in enumerate(all_objects):
        for other, previous in all_objects[:index]:
            if _overlap((item['start'], item['end']),
                        (previous['start'], previous['end'])):
                raise ValueError(f'{name} overlaps {other}')
    return {'objects': objects, 'boot_pool': pool, 'page_size': PAGE,
            'table_pages': 32, 'alias': ALIAS, 'probe': PROBE}


def _elf(data, raw, fault):
    if _range(data, 0, 16, 'ELF identification')[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected ELF64 little-endian version 1')
    kind, machine, version, entry, phoff, shoff, _, ehsize, phsize, phnum, shsize, shnum, names_index = \
        _unpack('<HHIQQQIHHHHHH', data, 16, 'ELF header')
    if (kind != 2 or machine != 183 or version != 1 or ehsize != 64 or
            not 0 < phnum < 0xffff or phsize != 56 or not shnum or shsize != 64 or
            not 0 < names_index < shnum):
        raise ValueError('unsupported AArch64 ELF layout')
    _range(data, phoff, phnum * phsize, 'ELF program headers')
    _range(data, shoff, shnum * shsize, 'ELF section headers')
    sections = [_unpack('<IIQQQQIIQQ', data, shoff + n * shsize, 'ELF section')
                for n in range(shnum)]
    for s in sections:
        if s[3] + s[5] > U64:
            raise ValueError('ELF section address overflows')
        if s[1] != 8:
            _range(data, s[4], s[5], 'ELF section data')
    ns = sections[names_index]
    if ns[1] != 3:
        raise ValueError('invalid ELF section string table')
    names = _range(data, ns[4], ns[5], 'ELF section names')
    symtabs = [s for s in sections if _string(names, s[0], 'section') == b'.symtab']
    if len(symtabs) != 1:
        raise ValueError('missing or duplicate ELF symbol table')
    table = symtabs[0]
    if table[1] != 2 or table[9] != 24 or table[5] % 24 or not 0 < table[6] < shnum:
        raise ValueError('invalid ELF symbol table layout')
    strings_section = sections[table[6]]
    if strings_section[1] != 3:
        raise ValueError('invalid ELF symbol string table')
    strings = _range(data, strings_section[4], strings_section[5], 'ELF symbols')
    wanted = {name for name, _ in (*OBJECTS.values(), *POOL.values())}
    wanted.update(('image_start', '__image_end', '__bss_start', '__bss_end', FAULT))
    symbols = {name: [] for name in wanted}
    for off in range(table[4], table[4] + table[5], 24):
        name, info, other, index, value, size = _unpack('<IBBHQQ', data, off, 'ELF symbol')
        label = _string(strings, name, 'symbol').decode('ascii')
        if label in symbols:
            symbols[label].append((info, other, index, value, size))
    if not fault and symbols[FAULT]:
        raise ValueError('fault injection present in positive paging image')

    def symbol(name, types):
        if len(symbols[name]) != 1:
            raise ValueError(f'missing or duplicate ELF symbol {name}')
        info, other, index, value, size = symbols[name][0]
        if info >> 4 != 1 or info & 15 not in types or other & ~3 or not 0 < index < shnum:
            raise ValueError(f'suspicious ELF symbol {name}')
        return index, value, size

    loads = []
    for n in range(phnum):
        p = _unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF segment')
        if p[0] == 1:
            if p[5] > p[6] or p[3] + p[6] > U64 or p[4] + p[6] > U64:
                raise ValueError('invalid ELF PT_LOAD extent')
            _range(data, p[2], p[5], 'ELF PT_LOAD bytes')
            loads.append(p)

    def mapping(bounds, flags, backed=False):
        matches = [p for p in loads if _overlap(bounds, (p[3], p[3] + p[6]))]
        if (len(matches) != 1 or matches[0][1] & flags != flags or
                matches[0][3] != matches[0][4] or
                not _inside(bounds, (matches[0][3], matches[0][3] + matches[0][6])) or
                backed and not _inside(bounds, (matches[0][3], matches[0][3] + matches[0][5]))):
            raise ValueError('no unique compatible full identity PT_LOAD')
        return matches[0]

    image_start = symbol('image_start', (0, 2))[1]
    image_end = symbol('__image_end', (0, 2))[1]
    bss_start = symbol('__bss_start', (0, 2))[1]
    bss_end = symbol('__bss_end', (0, 2))[1]
    image = _interval(image_start, image_end - image_start, 'ARM image')
    if image_start != ARM_BASE or entry != image_start:
        raise ValueError('unexpected ARM image entry')
    if (bss_start | bss_end) & 7 or not _inside((bss_start, bss_end), image):
        raise ValueError('invalid BSS clear interval')

    def resolve(name, size, section_type, flags, exact_size=True):
        index, address, declared = symbol(name, (1,) if exact_size else (0, 2))
        bounds = _interval(address, size, name)
        s = sections[index]
        if (exact_size and declared != size or s[1] != section_type or
                s[2] & flags != flags or not _inside(bounds, image) or
                not _inside(bounds, (s[3], s[3] + s[5]))):
            raise ValueError(f'{name} is not wholly in its expected ELF section')
        p = mapping(bounds, 6 if flags & 1 else 5, section_type != 8)
        offset = s[4] + address - s[3]
        if section_type != 8 and p[2] + address - p[3] != offset:
            raise ValueError('ELF section and segment bytes disagree')
        return bounds, s, offset

    header_bounds, _, header_offset = resolve('image_start', 64, 1, 6, False)
    mapping(header_bounds, 1, True)
    header = _range(raw, 0, 64, 'raw Image header')
    if (header != _range(data, header_offset, 64, 'ELF Image header') or
            _unpack('<Q', header, 8, 'Image offset')[0] != 0x80000 or
            _unpack('<Q', header, 16, 'Image size')[0] != image_end - image_start or
            _unpack('<I', header, 56, 'Image magic')[0] != 0x644d5241 or
            len(raw) > image_end - image_start):
        raise ValueError('raw Image header/size disagrees with linked image')
    objects, pool = {}, {}
    for key, (name, size) in {**OBJECTS, **POOL}.items():
        initialized = key == 'data'
        bounds, section, offset = resolve(name, size, 1 if initialized else 8, 3)
        if initialized:
            if _range(data, offset, size, 'paging initial data') != INITIAL:
                raise ValueError('paging data initializer is not the complete expected page')
        elif not _inside(bounds, (bss_start, bss_end)):
            raise ValueError(f'{name} lies outside BSS clear extent')
        item = dict(symbol=name, start=bounds[0], end=bounds[1], bytes=size,
                    section=_string(names, section[0], 'section').decode('ascii'))
        (objects if key in OBJECTS else pool)[key] = item
    # Bind every loaded file-backed byte to the actual raw transport image.
    for s in sections:
        if s[1] != 8 and s[2] & 2 and s[5]:
            bounds = _interval(s[3], s[5], 'allocated ELF section')
            if not _inside(bounds, image) or _range(raw, s[3] - image_start, s[5], 'raw section') != \
                    _range(data, s[4], s[5], 'ELF section'):
                raise ValueError('allocated ELF/raw section mismatch')
    result = _finish(objects, pool)
    if fault:
        instruction = INSTRUCTION['aarch64']
        bounds, _, offset = resolve(FAULT, len(instruction), 1, 6, False)
        if bounds[0] % 4 or _range(data, offset, len(instruction), 'fault instruction') != instruction:
            raise ValueError('paging fault symbol does not name the expected read instruction')
        result['fault_site'] = dict(symbol=FAULT, pc=bounds[0], offset=offset,
                                    image_offset=bounds[0] - image_start,
                                    instruction_hex=instruction.hex())
    result.update(arch='aarch64', address_space='identity-image-address', fault=fault,
                  image_start=image_start, image_end=image_end,
                  image_size=image_end - image_start, bss_start=bss_start, bss_end=bss_end)
    return result


def _pe(data, map_bytes, fault):
    if _range(data, 0, 64, 'DOS header')[:2] != b'MZ':
        raise ValueError('expected PE DOS header')
    pe = _unpack('<I', data, 60, 'PE header offset')[0]
    if pe < 64 or _range(data, pe, 4, 'PE signature') != b'PE\0\0':
        raise ValueError('invalid PE signature')
    machine, count, _, _, _, optional_size, _ = _unpack('<HHIIIHH', data, pe + 4, 'COFF header')
    if machine != 0x8664 or not 0 < count <= 96 or optional_size < 112:
        raise ValueError('unsupported AMD64 PE layout')
    optional = _range(data, pe + 24, optional_size, 'PE optional header')
    if _unpack('<H', optional, 0, 'PE magic')[0] != 0x20b:
        raise ValueError('expected PE32+')
    base = _unpack('<Q', optional, 24, 'PE ImageBase')[0]
    image_size, headers = _unpack('<II', optional, 56, 'PE image bounds')
    section_offset = pe + 24 + optional_size
    _range(data, section_offset, count * 40, 'PE section table')
    if not image_size or base + image_size > U64 or not section_offset + count * 40 <= headers <= len(data):
        raise ValueError('invalid PE image/header bounds')
    sections = []
    for n in range(count):
        s = _unpack('<8sIIIIIIHHI', data, section_offset + n * 40, 'PE section')
        if s[2] + (s[1] or s[3]) > min(U32, image_size):
            raise ValueError('PE section exceeds SizeOfImage')
        if s[3]:
            if s[4] < headers:
                raise ValueError('PE raw section overlaps headers')
            _range(data, s[4], s[3], 'PE section bytes')
        sections.append(s)
    if b'\0' in map_bytes:
        raise ValueError('NUL in linker map')
    lines = map_bytes.decode('utf-8').splitlines()
    heading = [n for n, line in enumerate(lines) if re.fullmatch(
        r'\s*Address\s+Publics by Value\s+Rva\+Base\s+Lib:Object\s*', line)]
    if len(heading) != 1:
        raise ValueError('missing or duplicate linker map heading')
    preferred = [line for line in lines if 'Preferred load address is' in line]
    if preferred:
        match = re.fullmatch(r'\s*Preferred load address is ([0-9a-fA-F]{1,16})\s*', preferred[0])
        if len(preferred) != 1 or not match or int(match[1], 16) != base:
            raise ValueError('PE/map preferred base mismatch')

    def rows(name):
        return [(n, line.split()) for n, line in enumerate(lines) if name in line.split()]

    if not fault and rows(FAULT):
        raise ValueError('fault injection present in positive paging image')

    def resolve(name, size, executable=False):
        matches = rows(name)
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate map symbol {name}')
        number, fields = matches[0]
        if (number <= heading[0] or len(fields) < 4 or fields[1] != name or
                not re.fullmatch(r'[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,16}', fields[0]) or
                not re.fullmatch(r'[0-9a-fA-F]{8,16}', fields[2])):
            raise ValueError('invalid linker map symbol row')
        segment, relative = (int(x, 16) for x in fields[0].split(':'))
        address = int(fields[2], 16)
        if address < base:
            raise ValueError('map symbol below PE preferred base')
        bounds = _interval(address - base, size, name, U32)
        if bounds[1] > image_size:
            raise ValueError(f'{name} exceeds PE SizeOfImage')
        matches = [(n, s) for n, s in enumerate(sections, 1)
                   if _overlap(bounds, (s[2], s[2] + (s[1] or s[3])))]
        if len(matches) != 1:
            raise ValueError('object has no unique PE section')
        n, s = matches[0]
        if (segment != n or relative != bounds[0] - s[2] or
                not _inside(bounds, (s[2], s[2] + (s[1] or s[3]))) or
                executable and not s[9] & 0x20000000 or
                not executable and (not s[9] & 0x80000000 or not s[9] & 0xc0)):
            raise ValueError('object does not fit its matching PE section/type')
        return bounds, s, relative

    objects, pool = {}, {}
    for key, (name, size) in {**OBJECTS, **POOL}.items():
        bounds, section, relative = resolve(name, size)
        present = max(0, min(size, section[3] - relative))
        if key == 'data':
            if present != size or _range(data, section[4] + relative, size, 'paging initializer') != INITIAL:
                raise ValueError('paging data is not fully file-backed with expected initializer')
        elif present and any(_range(data, section[4] + relative, present, 'zero-fill object')):
            raise ValueError('paging BSS reserve has nonzero initial bytes')
        item = dict(symbol=name, start=bounds[0], end=bounds[1], bytes=size,
                    section=section[0].split(b'\0', 1)[0].decode('ascii'))
        (objects if key in OBJECTS else pool)[key] = item
    result = _finish(objects, pool)
    if fault:
        instruction = INSTRUCTION['x86_64']
        bounds, section, relative = resolve(FAULT, len(instruction), True)
        if relative + len(instruction) > section[3]:
            raise ValueError('fault instruction is not file-backed')
        offset = section[4] + relative
        if _range(data, offset, len(instruction), 'fault read') != instruction:
            raise ValueError('paging fault symbol does not name the expected read instruction')
        result['fault_site'] = dict(symbol=FAULT, rva=bounds[0], offset=offset,
                                    instruction_hex=instruction.hex())
    result.update(arch='x86_64', address_space='image-rva', fault=fault,
                  preferred_image_base=base, image_start=0, image_end=image_size,
                  image_size=image_size)
    return result


def describe_paging(arch, binary, map_path=None, raw_path=None, fault=False):
    """Return whole-span static proof, requiring runtime corroboration by caller."""
    if arch not in INSTRUCTION or type(fault) is not bool:
        raise ValueError('unsupported paging architecture or fault mode')
    try:
        data = _read_file(binary, 'binary')
        if arch == 'aarch64':
            if raw_path is None:
                raise ValueError('AArch64 paging proof requires raw Image')
            raw = _read_file(raw_path, 'raw Image')
            result = _elf(data, raw, fault)
            result['raw_sha256'] = hashlib.sha256(raw).hexdigest()
        else:
            if map_path is None:
                raise ValueError('PE paging proof requires linker map')
            map_bytes = _read_file(map_path, 'linker map')
            result = _pe(data, map_bytes, fault)
            result['map_sha256'] = hashlib.sha256(map_bytes).hexdigest()
        result['binary_sha256'] = hashlib.sha256(data).hexdigest()
        return result
    except (OSError, UnicodeError, struct.error) as error:
        raise ValueError(f'cannot describe paging image: {error}') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=tuple(INSTRUCTION), required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--map', dest='map_path', type=Path)
    parser.add_argument('--raw', dest='raw_path', type=Path)
    parser.add_argument('--fault', action='store_true')
    args = parser.parse_args()
    try:
        result = describe_paging(args.arch, args.binary, args.map_path, args.raw_path, args.fault)
    except ValueError as error:
        parser.exit(1, f'paging-site error: {error}\n')
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
