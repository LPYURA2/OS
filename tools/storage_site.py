#!/usr/bin/env python3
"""Bind the fixed ARM storage profile to complete image-owned resources.

This proof uses ELF/raw Image bytes and the standard library only. It does not
claim to translate arbitrary C pointers to physical addresses: the guest must
separately establish the supported fixed Image, EL1, MMU/cache-off contract.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shlex

try:
    from .fault_site import _range, _unpack, _string, _read_file
    from .pool_site import describe_pool, _interval, _inside, _intersects
except ImportError:
    from fault_site import _range, _unpack, _string, _read_file
    from pool_site import describe_pool, _interval, _inside, _intersects

PAGE = 4096
COMMON = {
    'vblk_control': ('styleos_vblk_control', PAGE, PAGE),
    'queue': ('styleos_vblk_queue', PAGE, PAGE),
    'request': ('styleos_vblk_request', PAGE, PAGE),
    'control': ('styleos_storage_control', PAGE, PAGE),
    'workspace': ('styleos_storage_workspace', PAGE, PAGE),
    'diagnostic': ('diagnostic', 12400, 8),
}
FORBIDDEN_PREFIXES = (b'styleos_tasks_', b'styleos_user_', b'styleos_paging_')
FORBIDDEN = frozenset((b'styleos_test_fault', b'styleos_ownership_workspace',
                       b'tasks_arch_switch'))


def _flags(flags):
    if isinstance(flags, str):
        try:
            flags = shlex.split(flags)
        except ValueError as error:
            raise ValueError('malformed storage compiler flags') from error
    if (not isinstance(flags, (tuple, list)) or not flags or
            any(not isinstance(flag, str) or not flag or '\x00' in flag for flag in flags)):
        raise ValueError('storage compiler flags must be a nonempty string or list')
    flags = list(flags)
    define = '-DSTYLEOS_TEST_STORAGE=1'
    target = '--target=aarch64-none-elf'
    expected = '-DSTYLEOS_EXPECT_ARCH=2'
    if (flags.count(target) != 1 or flags.count(expected) != 1 or
            flags.count(define) != 1 or
            any((flag.startswith(('--target', '-target')) and flag != target) or
                (flag.startswith('-DSTYLEOS_EXPECT_ARCH') and flag != expected) or
                flag.startswith('-USTYLEOS_') or
                (flag.startswith('-DSTYLEOS_TEST_') and flag != define) or
                (flag in ('-D', '-U') and index + 1 < len(flags) and
                 flags[index + 1].startswith('STYLEOS_'))
                for index, flag in enumerate(flags))):
        raise ValueError('storage compiler target/profile mismatch')
    return flags


def _item(symbol, bounds):
    return dict(symbol=symbol, start=bounds[0], end=bounds[1],
                bytes=bounds[1] - bounds[0])


def _objects(data, pool):
    # describe_pool has already validated the same hashed header, symbols,
    # allocated contents, Image header and whole pool/BSS/PT_LOAD intervals.
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
        if label in FORBIDDEN or label.startswith(FORBIDDEN_PREFIXES):
            raise ValueError('unrelated experimental profile in storage image')
        symbols.setdefault(label, []).append((info, other, index, value, size))
    loads = [_unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF program header')
             for n in range(phnum)]
    loads = [p for p in loads if p[0] == 1]
    image = pool['image_start'], pool['image_end']
    bss = pool['bss_start'], pool['bss_end']
    allocated = [s for s in sections if s[2] & 2 and s[5]]
    load_bounds = []
    for p in loads:
        bounds = _interval(p[3], p[6], 'storage LOAD')
        alignment = p[7]
        if (p[3] != p[4] or not _inside(bounds, image) or p[1] & ~7 or
                (alignment > 1 and
                 (alignment & (alignment - 1) or p[2] % alignment != p[3] % alignment)) or
                any(_intersects(bounds, previous) for previous in load_bounds)):
            raise ValueError('storage LOAD is outside Image, nonidentity, unaligned or overlapping')
        claimed = [s for s in allocated if
                   _inside((s[3], s[3] + s[5]), bounds)]
        if (not claimed or min(s[3] for s in claimed) != bounds[0] or
                max(s[3] + s[5] for s in claimed) != bounds[1]):
            raise ValueError('storage LOAD has an unclaimed image extent')
        load_bounds.append(bounds)

    # The complete loader-cleared BSS, not just the DMA start addresses, must
    # be covered by one zero-filled, writable identity LOAD segment.
    bss_loads = [p for p in loads if _intersects(bss, (p[3], p[3] + p[6]))]
    if (len(bss_loads) != 1 or bss_loads[0][1] & 6 != 6 or
            bss_loads[0][3] != bss_loads[0][4] or
            not _inside(bss, (bss_loads[0][3], bss_loads[0][3] + bss_loads[0][6])) or
            bss[0] < bss_loads[0][3] + bss_loads[0][5]):
        raise ValueError('complete storage BSS has no unique zero-filled identity LOAD')

    def symbol(name, types, local=False):
        matches = symbols.get(name.encode(), [])
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate storage ELF symbol {name}')
        info, other, index, value, size = matches[0]
        if (info >> 4 not in ((0, 1) if local else (1,)) or
                info & 15 not in types or other & ~3 or not 0 < index < shnum):
            raise ValueError(f'suspicious storage ELF symbol {name}')
        return index, value, size

    def resolve(name, size, alignment, *, code=False, endpoint=None, local=False):
        types = (0,) if endpoint else (0, 1, 2) if code else (1,)
        index, address, declared = symbol(name, types, local)
        bounds = _interval(address, size, name)
        section = sections[index]
        if address % alignment:
            raise ValueError(f'{name} is not aligned to its full storage contract')
        if endpoint:
            end_index, end, end_size = symbol(endpoint, (0,))
            if end_index != index or end != bounds[1] or declared or end_size:
                raise ValueError('kernel stack endpoints do not cover its complete extent')
        elif declared != size:
            raise ValueError(f'{name} has the wrong complete object size')
        wanted_flags = 6 if code else 3
        if (section[1] != (1 if code else 8) or section[2] & wanted_flags != wanted_flags or
                not _inside(bounds, (section[3], section[3] + section[5])) or
                not _inside(bounds, image)):
            raise ValueError(f'{name} is outside its complete allocated storage section')
        matches = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(matches) != 1 or matches[0][1] & (5 if code else 6) != (5 if code else 6) or
                matches[0][3] != matches[0][4] or
                not _inside(bounds, (matches[0][3], matches[0][3] + matches[0][6]))):
            raise ValueError(f'{name} has no complete unique identity LOAD')
        p = matches[0]
        if code:
            if (not _inside(bounds, (p[3], p[3] + p[5])) or
                    p[2] + address - p[3] != section[4] + address - section[3]):
                raise ValueError('vector file bytes and executable LOAD disagree')
        elif not _inside(bounds, bss) or address < p[3] + p[5]:
            raise ValueError(f'{name} is outside the fully zero-filled BSS')
        return _item(name, bounds)

    objects = {key: resolve(name, size, align, local=key == 'diagnostic')
               for key, (name, size, align) in COMMON.items()}
    objects['stack'] = resolve('__stack_bottom', 65536, 16, endpoint='__stack_top')
    objects['vectors'] = resolve('aarch64_vectors', 2048, 2048, code=True)
    objects['pool'] = _item('styleos_bootpool_storage',
                            (pool['pool_start'], pool['pool_end']))
    objects['pool_metadata'] = _item('styleos_bootpool_metadata',
                                     (pool['metadata_start'], pool['metadata_end']))
    rows = list(objects.items())
    for n, (name, item) in enumerate(rows):
        for other, previous in rows[:n]:
            if _intersects((item['start'], item['end']),
                           (previous['start'], previous['end'])):
                raise ValueError(f'storage object {name} overlaps {other}')
    # Validate each allocated section against unique identity LOAD bytes too;
    # a legitimate pool does not authenticate a different section elsewhere.
    for section in allocated:
        bounds = _interval(section[3], section[5], 'allocated storage section')
        candidates = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(candidates) != 1 or candidates[0][3] != candidates[0][4] or
                not _inside(bounds, image) or
                not _inside(bounds, (candidates[0][3], candidates[0][3] + candidates[0][6]))):
            raise ValueError('allocated storage section has no unique full identity LOAD')
        p = candidates[0]
        if section[1] != 8 and (not _inside(bounds, (p[3], p[3] + p[5])) or
                p[2] + section[3] - p[3] != section[4]):
            raise ValueError('allocated storage section and LOAD file bytes disagree')
    executable = [(section[3], section[3] + section[5]) for section in sections
                  if section[1] == 1 and section[2] & 6 == 6 and section[5]]
    start_index, start, start_size = symbol('_start', (0, 2))
    start_section = sections[start_index]
    if (start_size or start_section[1] != 1 or start_section[2] & 6 != 6 or
            not _inside(_interval(start, 4, 'storage entry'),
                        (start_section[3], start_section[3] + start_section[5]))):
        raise ValueError('storage _start is not a unique executable entry label')
    return objects, executable, start


def describe_storage(arch, binary, map_path=None, raw_path=None, compiler_flags=None):
    """Prove finite storage buffers from exact ELF/raw Image bytes or refuse."""
    if arch != 'aarch64' or raw_path is None or map_path is not None:
        raise ValueError('storage proof supports only AArch64 ELF plus raw Image')
    flags = _flags(compiler_flags)
    pool = describe_pool(arch, binary, raw_path=raw_path)
    data, raw = _read_file(binary, 'storage ELF'), _read_file(raw_path, 'storage raw Image')
    if (hashlib.sha256(data).hexdigest() != pool['binary_sha256'] or
            hashlib.sha256(raw).hexdigest() != pool['raw_sha256']):
        raise ValueError('storage image changed during binary proof')
    objects, executable, start = _objects(data, pool)
    branch = _unpack('<I', raw, 0, 'Image entry branch')[0]
    immediate = branch & 0x3ffffff
    if immediate & 0x2000000:
        immediate -= 0x4000000
    target = pool['image_start'] + immediate * 4
    if (branch & 0xfc000000 != 0x14000000 or
            target != start or
            not pool['image_start'] + 64 <= target < pool['image_end'] or
            not any(start <= target < target + 4 <= end for start, end in executable)):
        raise ValueError('storage Image entry does not branch to its executable _start')
    result = {key: pool[key] for key in ('arch', 'address_space', 'image_start',
        'image_end', 'image_size', 'bss_start', 'bss_end', 'binary_sha256', 'raw_sha256')}
    result.update(objects=objects, page_size=PAGE, compiler_flags=flags,
                  image_entry_branch=branch, image_entry_target=target,
                  runtime_contract='fixed ARM Image; EL1h; MMU, data and instruction caches off')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--raw', type=Path, required=True)
    parser.add_argument('--compiler-flags', required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    text = json.dumps(describe_storage('aarch64', args.binary,
        raw_path=args.raw, compiler_flags=args.compiler_flags), indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
