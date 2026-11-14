#!/usr/bin/env python3
"""Prove image-owned storage for the read-only ownership-plan profiles.

PE coordinates are RVAs and require the observed Loaded Image base at runtime.
ARM coordinates apply only to the supported identity-addressed QEMU Image.
The associated linker map supplies PE symbols; its hash binds the sidecar to
the build evidence, rather than authenticating an arbitrary foreign binary.
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
    from .pool_site import describe_pool, _interval, _inside, _intersects
except ImportError:
    from fault_site import _range, _unpack, _string, _read_file
    from pool_site import describe_pool, _interval, _inside, _intersects

PAGE = 4096
WORKSPACE_SYMBOL = 'styleos_ownership_workspace'
WORKSPACE_BYTES = 65536
DIAGNOSTIC_BYTES = 12400
STACK_BYTES = 65536
FORBIDDEN = frozenset(('styleos_paging_tables', 'styleos_paging_control',
                       'styleos_paging_data', 'styleos_test_fault',
                       'styleos_paging_fault'))
COMMON = {'workspace': (WORKSPACE_SYMBOL, WORKSPACE_BYTES, PAGE),
          'diagnostic': ('diagnostic', DIAGNOSTIC_BYTES, 8)}
STATIC_ORIGINS = {'diagnostic': 'kernel_boot_memory_diagnostic.o',
                  'handoff': 'boot_x86_64_uefi.o',
                  'early_idt': 'arch_x86_64_exceptions.o'}


def _item(symbol, bounds):
    return dict(symbol=symbol, start=bounds[0], end=bounds[1],
                bytes=bounds[1] - bounds[0])


def _finish(pool, objects, invalid):
    objects['pool'] = _item('styleos_bootpool_storage',
                            (pool['pool_start'], pool['pool_end']))
    objects['pool_metadata'] = _item('styleos_bootpool_metadata',
                                     (pool['metadata_start'], pool['metadata_end']))
    rows = list(objects.items())
    for n, (name, item) in enumerate(rows):
        for other, previous in rows[:n]:
            if _intersects((item['start'], item['end']),
                           (previous['start'], previous['end'])):
                raise ValueError(f'{name} overlaps {other}')
    keys = ('arch', 'address_space', 'image_start', 'image_end', 'image_size',
            'preferred_image_base', 'bss_start', 'bss_end',
            'binary_sha256', 'map_sha256', 'raw_sha256')
    result = {key: pool[key] for key in keys if key in pool}
    result.update(objects=objects, page_size=PAGE, invalid=invalid)
    return result


def _elf_objects(data, pool):
    # describe_pool has validated this header, all section contents, the symbol
    # table and the raw Image. Parse the same hashed bytes to add our objects.
    h = _unpack('<HHIQQQIHHHHHH', data, 16, 'ELF header')
    phoff, shoff, phsize, phnum, shsize, shnum, names_index = \
        h[4], h[5], h[8], h[9], h[10], h[11], h[12]
    sections = [_unpack('<IIQQQQIIQQ', data, shoff + n * shsize, 'ELF section')
                for n in range(shnum)]
    ns = sections[names_index]
    names = _range(data, ns[4], ns[5], 'ELF section names')
    table = next(s for s in sections if _string(names, s[0], 'section') == b'.symtab')
    ss = sections[table[6]]
    strings = _range(data, ss[4], ss[5], 'ELF symbol names')
    symbols = {}
    for off in range(table[4], table[4] + table[5], 24):
        name, info, other, index, value, size = _unpack('<IBBHQQ', data, off, 'ELF symbol')
        label = _string(strings, name, 'symbol')
        if label in {name.encode() for name in FORBIDDEN}:
            raise ValueError('paging buffer or managed fault present in ownership image')
        symbols.setdefault(label, []).append((info, other, index, value, size))
    loads = [_unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF program header')
             for n in range(phnum)]
    loads = [p for p in loads if p[0] == 1]
    image = pool['image_start'], pool['image_end']
    bss = pool['bss_start'], pool['bss_end']

    def symbol(name, types=(1,), local=False):
        matches = symbols.get(name.encode(), [])
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate ELF symbol {name}')
        info, other, index, value, size = matches[0]
        if (info >> 4 not in ((0, 1) if local else (1,)) or
                info & 15 not in types or other & ~3 or not 0 < index < shnum):
            raise ValueError(f'suspicious ELF symbol {name}')
        return index, value, size

    def resolve(name, size, alignment, *, code=False, endpoint=None, local=False):
        index, address, declared = symbol(name, (0,) if endpoint else (1,), local)
        bounds = _interval(address, size, name)
        section = sections[index]
        if address % alignment:
            raise ValueError(f'{name} is not correctly aligned')
        if endpoint:
            end_index, end, end_size = symbol(endpoint, (0,))
            if end != bounds[1] or end_index != index or declared or end_size:
                raise ValueError('stack boundaries do not describe the complete 65536-byte stack')
        elif declared != size:
            raise ValueError(f'{name} has unexpected ELF symbol size')
        flags = 6 if code else 3
        if (section[1] != (1 if code else 8) or section[2] & flags != flags or
                not _inside(bounds, (section[3], section[3] + section[5])) or
                not _inside(bounds, image)):
            raise ValueError(f'{name} is not wholly in its expected ELF section/image')
        matches = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(matches) != 1 or matches[0][1] & (5 if code else 6) != (5 if code else 6)
                or matches[0][3] != matches[0][4] or
                not _inside(bounds, (matches[0][3], matches[0][3] + matches[0][6]))):
            raise ValueError(f'{name} has no unique complete identity PT_LOAD')
        p = matches[0]
        if code:
            if (not _inside(bounds, (p[3], p[3] + p[5])) or
                    p[2] + address - p[3] != section[4] + address - section[3]):
                raise ValueError(f'{name} executable section and PT_LOAD bytes disagree')
        elif not _inside(bounds, bss) or address < p[3] + p[5]:
            raise ValueError(f'{name} is outside the complete zero-filled BSS extent')
        return _item(name, bounds)

    objects = {key: resolve(name, size, alignment, local=key == 'diagnostic')
               for key, (name, size, alignment) in COMMON.items()}
    objects['stack'] = resolve('__stack_bottom', STACK_BYTES, 16, endpoint='__stack_top')
    objects['vectors'] = resolve('aarch64_vectors', 2048, 2048, code=True)
    return objects


def _pe_objects(data, map_bytes, pool):
    pe = _unpack('<I', data, 60, 'PE offset')[0]
    count = _unpack('<H', data, pe + 6, 'PE section count')[0]
    optional_size = _unpack('<H', data, pe + 20, 'PE optional size')[0]
    section_offset = pe + 24 + optional_size
    sections = [_unpack('<8sIIIIIIHHI', data, section_offset + n * 40, 'PE section')
                for n in range(count)]
    lines = map_bytes.decode('utf-8').splitlines()
    public = next(n for n, line in enumerate(lines) if re.fullmatch(
        r'\s*Address\s+Publics by Value\s+Rva\+Base\s+Lib:Object\s*', line))
    static = [n for n, line in enumerate(lines) if re.fullmatch(r'\s*Static symbols\s*', line)]
    if len(static) != 1 or static[0] <= public:
        raise ValueError('missing or duplicate linker map Static symbols heading')
    for line in lines:
        if FORBIDDEN.intersection(line.split()):
            raise ValueError('paging buffer or managed fault present in ownership image')
    base, image_size = pool['preferred_image_base'], pool['image_size']

    def symbol(name):
        matches = [(n, line.split()) for n, line in enumerate(lines)
                   if name in line.split()]
        origin = STATIC_ORIGINS.get(name)
        if origin:
            matches = [(n, fields) for n, fields in matches
                       if fields[-1].replace('\\', '/').rsplit('/', 1)[-1] == origin]
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate linker map symbol {name}')
        row, fields = matches[0]
        if (row <= public or (row <= static[0] if origin else row >= static[0]) or
                len(fields) < 4 or fields[1] != name or
                not re.fullmatch(r'[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,16}', fields[0]) or
                not re.fullmatch(r'[0-9a-fA-F]{8,16}', fields[2])):
            raise ValueError(f'suspicious linker map symbol row {name}')
        segment, relative = (int(v, 16) for v in fields[0].split(':'))
        absolute = int(fields[2], 16)
        if absolute < base:
            raise ValueError('linker map address is below preferred ImageBase')
        return segment, relative, absolute - base

    def resolve(name, size, alignment, *, endpoint=None, initialized=False):
        segment, relative, address = symbol(name)
        bounds = _interval(address, size, name, 1 << 32)
        if address % alignment or bounds[1] > image_size:
            raise ValueError(f'{name} is unaligned or extends beyond SizeOfImage')
        if endpoint:
            end_segment, end_relative, end = symbol(endpoint)
            if end != bounds[1] or end_segment != segment or end_relative != relative + size:
                raise ValueError('stack boundaries do not describe the complete 65536-byte stack')
        matches = [(n, s) for n, s in enumerate(sections, 1)
                   if _intersects(bounds, (s[2], s[2] + (s[1] or s[3])))]
        if (len(matches) != 1 or
                not _inside(bounds, (matches[0][1][2], matches[0][1][2] +
                                      (matches[0][1][1] or matches[0][1][3])))):
            raise ValueError(f'{name} has no unique complete PE section')
        number, s = matches[0]
        if (segment != number or relative != address - s[2] or
                not s[9] & 0x80000000 or not s[9] & 0xc0):
            raise ValueError(f'{name} is not in matching writable PE data')
        present = max(0, min(size, s[3] - relative))
        if initialized:
            if present != size:
                raise ValueError('handoff is not completely backed by initialized PE bytes')
        elif present and any(_range(data, s[4] + relative, present, 'PE BSS prefix')):
            raise ValueError(f'{name} has nonzero initialized bytes')
        return _item(name, bounds)

    objects = {key: resolve(name, size, alignment)
               for key, (name, size, alignment) in COMMON.items()}
    objects['stack'] = resolve('styleos_stack_bottom', STACK_BYTES, 16,
                               endpoint='styleos_stack_top')
    objects['idt'] = resolve('early_idt', 4096, 16)
    objects['handoff'] = resolve('handoff', 96, 8, initialized=True)
    return objects


def describe_ownership(arch, binary, map_path=None, raw_path=None, invalid=False):
    """Return complete verified image-object intervals and hashes, or ValueError.

    ``invalid`` identifies the controlled runtime rejection profile; it never
    relaxes the static layout proof. No result interprets a PE preferred base
    as its runtime physical placement or declares general RAM owned.
    """
    if not isinstance(invalid, bool):
        raise ValueError('invalid profile selector must be a bool')
    try:
        pool = describe_pool(arch, binary, map_path, raw_path)
        data = _read_file(binary, 'binary')
        if hashlib.sha256(data).hexdigest() != pool['binary_sha256']:
            raise ValueError('binary changed during ownership proof')
        if arch == 'aarch64':
            raw = _read_file(raw_path, 'raw Image')
            if hashlib.sha256(raw).hexdigest() != pool['raw_sha256']:
                raise ValueError('raw Image changed during ownership proof')
            objects = _elf_objects(data, pool)
        else:
            map_bytes = _read_file(map_path, 'linker map')
            if hashlib.sha256(map_bytes).hexdigest() != pool['map_sha256']:
                raise ValueError('linker map changed during ownership proof')
            objects = _pe_objects(data, map_bytes, pool)
        return _finish(pool, objects, invalid)
    except (OSError, UnicodeError, struct.error) as error:
        raise ValueError(f'cannot describe ownership site: {error}') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=('aarch64', 'x86_64'), required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--map', dest='map_path', type=Path)
    parser.add_argument('--raw', dest='raw_path', type=Path)
    parser.add_argument('--invalid', action='store_true')
    args = parser.parse_args()
    try:
        result = describe_ownership(args.arch, args.binary, args.map_path, args.raw_path, args.invalid)
    except ValueError as error:
        parser.exit(1, f'ownership-site error: {error}\n')
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
