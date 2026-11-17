#!/usr/bin/env python3
"""Bind the fixed ARM disk-app profile to complete image-owned resources.

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
import struct
import zlib

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
    'control': ('styleos_disk_app_control', 16384, PAGE),
    'workspace': ('styleos_disk_app_storage_workspace', PAGE, PAGE),
    'tables': ('styleos_disk_app_tables', 2 * 32 * PAGE, PAGE),
    'entry_stack': ('styleos_disk_app_entry_stack', 16384, PAGE),
    'code': ('styleos_disk_app_code', PAGE, PAGE),
    'user_stack': ('styleos_disk_app_user_stack', PAGE, PAGE),
    'diagnostic': ('diagnostic', 12400, 8),
}
FORBIDDEN_PREFIXES = (b'styleos_tasks_', b'styleos_user_', b'styleos_paging_', b'disk_app_entry', b'disk_app_fault', b'disk_app_normal', b'disk_app_exit', b'disk_app_end')
FORBIDDEN = frozenset((b'styleos_test_fault', b'styleos_ownership_workspace',
                       b'tasks_arch_switch', b'styleos_storage_control', b'styleos_storage_workspace'))
APP_MAGIC = b'STYAPP1\0'
APP_HEADER = 32
APP_CODE_BYTES = 28
USER_CODE_VA = 0x10000000
USER_STACK_VA = 0x11000000
FAULT_VA = 0x12010000
APP_SYMBOLS = {'disk_app_entry': 0, 'disk_app_fault': 8,
               'disk_app_normal': 12, 'disk_app_exit': 24, 'disk_app_end': 28}


def _flags(flags):
    if isinstance(flags, str):
        try:
            flags = shlex.split(flags)
        except ValueError as error:
            raise ValueError('malformed disk-app compiler flags') from error
    if (not isinstance(flags, (tuple, list)) or not flags or
            any(not isinstance(flag, str) or not flag or '\x00' in flag for flag in flags)):
        raise ValueError('disk-app compiler flags must be a nonempty string or list')
    flags = list(flags)
    define = '-DSTYLEOS_TEST_DISK_APP=1'
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
        raise ValueError('disk-app compiler target/profile mismatch')
    if (any(flags.count(option) != 1 for option in ('-mgeneral-regs-only', '-mstrict-align')) or
            any(option in ('-mgeneral-regs-only=0', '-mno-general-regs-only',
                           '-mno-strict-align', '-mstrict-align=0', '-mhard-float') or
                option.startswith(('@', '-march=', '-mcpu=', '-mfpu=', '-mfloat-abi=',
                                   '-Xclang', '-mllvm')) for option in flags)):
        raise ValueError('disk-app requires integer-only strict-alignment compiler profile')
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
            raise ValueError('unrelated experimental profile in disk-app image')
        symbols.setdefault(label, []).append((info, other, index, value, size))
    loads = [_unpack('<IIQQQQQQ', data, phoff + n * phsize, 'ELF program header')
             for n in range(phnum)]
    loads = [p for p in loads if p[0] == 1]
    image = pool['image_start'], pool['image_end']
    bss = pool['bss_start'], pool['bss_end']
    allocated = [s for s in sections if s[2] & 2 and s[5]]
    load_bounds = []
    for p in loads:
        bounds = _interval(p[3], p[6], 'disk-app LOAD')
        alignment = p[7]
        if (p[3] != p[4] or not _inside(bounds, image) or p[1] & ~7 or
                (alignment > 1 and
                 (alignment & (alignment - 1) or p[2] % alignment != p[3] % alignment)) or
                any(_intersects(bounds, previous) for previous in load_bounds)):
            raise ValueError('disk-app LOAD is outside Image, nonidentity, unaligned or overlapping')
        claimed = [s for s in allocated if
                   _inside((s[3], s[3] + s[5]), bounds)]
        if (not claimed or min(s[3] for s in claimed) != bounds[0] or
                max(s[3] + s[5] for s in claimed) != bounds[1]):
            raise ValueError('disk-app LOAD has an unclaimed image extent')
        load_bounds.append(bounds)

    # The complete loader-cleared BSS, not just the DMA start addresses, must
    # be covered by one zero-filled, writable identity LOAD segment.
    bss_loads = [p for p in loads if _intersects(bss, (p[3], p[3] + p[6]))]
    if (len(bss_loads) != 1 or bss_loads[0][1] != 6 or
            bss_loads[0][3] != bss_loads[0][4] or
            not _inside(bss, (bss_loads[0][3], bss_loads[0][3] + bss_loads[0][6])) or
            bss[0] < bss_loads[0][3] + bss_loads[0][5]):
        raise ValueError('complete disk-app BSS has no unique zero-filled identity LOAD')

    def symbol(name, types, local=False):
        matches = symbols.get(name.encode(), [])
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate disk-app ELF symbol {name}')
        info, other, index, value, size = matches[0]
        if (info >> 4 not in ((0, 1) if local else (1,)) or
                info & 15 not in types or other & ~3 or not 0 < index < shnum):
            raise ValueError(f'suspicious disk-app ELF symbol {name}')
        return index, value, size

    def resolve(name, size, alignment, *, code=False, endpoint=None, local=False):
        types = (0,) if endpoint else (0, 1, 2) if code else (1,)
        index, address, declared = symbol(name, types, local)
        bounds = _interval(address, size, name)
        section = sections[index]
        if address % alignment:
            raise ValueError(f'{name} is not aligned to its full disk-app contract')
        if endpoint:
            end_index, end, end_size = symbol(endpoint, (0,))
            if end_index != index or end != bounds[1] or declared or end_size:
                raise ValueError('kernel stack endpoints do not cover its complete extent')
        elif declared != size:
            raise ValueError(f'{name} has the wrong complete object size')
        wanted_flags = 6 if code else 3
        if (section[1] != (1 if code else 8) or section[2] != wanted_flags or
                not _inside(bounds, (section[3], section[3] + section[5])) or
                not _inside(bounds, image)):
            raise ValueError(f'{name} is outside its complete allocated disk-app section')
        matches = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(matches) != 1 or matches[0][1] != (5 if code else 6) or
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
    objects['disk_app_vectors'] = resolve('styleos_disk_app_vectors', 2048, 2048, code=True)
    objects['pool'] = _item('styleos_bootpool_storage',
                            (pool['pool_start'], pool['pool_end']))
    objects['pool_metadata'] = _item('styleos_bootpool_metadata',
                                     (pool['metadata_start'], pool['metadata_end']))
    rows = list(objects.items())
    for n, (name, item) in enumerate(rows):
        for other, previous in rows[:n]:
            if _intersects((item['start'], item['end']),
                           (previous['start'], previous['end'])):
                raise ValueError(f'disk-app object {name} overlaps {other}')
    # Validate each allocated section against unique identity LOAD bytes too;
    # a legitimate pool does not authenticate a different section elsewhere.
    allocated_bounds = []
    for section in allocated:
        bounds = _interval(section[3], section[5], 'allocated disk-app section')
        if any(_intersects(bounds, previous) for previous in allocated_bounds):
            raise ValueError('allocated disk-app sections overlap')
        allocated_bounds.append(bounds)
        candidates = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (len(candidates) != 1 or candidates[0][3] != candidates[0][4] or
                not _inside(bounds, image) or
                not _inside(bounds, (candidates[0][3], candidates[0][3] + candidates[0][6]))):
            raise ValueError('allocated disk-app section has no unique full identity LOAD')
        p = candidates[0]
        if section[1] != 8 and (not _inside(bounds, (p[3], p[3] + p[5])) or
                p[2] + section[3] - p[3] != section[4]):
            raise ValueError('allocated disk-app section and LOAD file bytes disagree')
    executable = [(section[3], section[3] + section[5]) for section in sections
                  if section[1] == 1 and section[2] & 6 == 6 and section[5]]
    start_index, start, start_size = symbol('_start', (0, 2))
    start_section = sections[start_index]
    if (start_size or start_section[1] != 1 or start_section[2] & 6 != 6 or
            not _inside(_interval(start, 4, 'disk-app entry'),
                        (start_section[3], start_section[3] + start_section[5]))):
        raise ValueError('disk-app _start is not a unique executable entry label')
    return objects, executable, start


def _complete_raw_image(data, raw, pool):
    """Bind alignment gaps and the raw file extent as well as named sections."""
    h = _unpack('<HHIQQQIHHHHHH', data, 16, 'ELF header')
    sections = [_unpack('<IIQQQQIIQQ', data, h[5] + n * h[10], 'ELF section')
                for n in range(h[11])]
    stored = [s for s in sections if s[1] != 8 and s[2] & 2 and s[5]]
    size = max(s[3] + s[5] - pool['image_start'] for s in stored)
    if len(raw) != size:
        raise ValueError('disk-app raw Image has omitted or unclaimed trailing bytes')
    expected = bytearray(size)
    for section in stored:
        offset = section[3] - pool['image_start']
        expected[offset:offset+section[5]] = _range(data, section[4], section[5], 'ELF stored section')
    if bytes(expected) != raw:
        raise ValueError('disk-app raw Image has foreign gap bytes or altered ELF contents')


def describe_disk_app(arch, binary, map_path=None, raw_path=None, compiler_flags=None):
    """Prove finite disk-app buffers from exact ELF/raw Image bytes or refuse."""
    if arch != 'aarch64' or raw_path is None or map_path is not None:
        raise ValueError('disk-app proof supports only AArch64 ELF plus raw Image')
    flags = _flags(compiler_flags)
    pool = describe_pool(arch, binary, raw_path=raw_path)
    data, raw = _read_file(binary, 'disk-app ELF'), _read_file(raw_path, 'disk-app raw Image')
    if (hashlib.sha256(data).hexdigest() != pool['binary_sha256'] or
            hashlib.sha256(raw).hexdigest() != pool['raw_sha256']):
        raise ValueError('disk-app image changed during binary proof')
    objects, executable, start = _objects(data, pool)
    _complete_raw_image(data, raw, pool)
    branch = _unpack('<I', raw, 0, 'Image entry branch')[0]
    immediate = branch & 0x3ffffff
    if immediate & 0x2000000:
        immediate -= 0x4000000
    target = pool['image_start'] + immediate * 4
    if (branch & 0xfc000000 != 0x14000000 or
            target != start or
            not pool['image_start'] + 64 <= target < pool['image_end'] or
            not any(start <= target < target + 4 <= end for start, end in executable)):
        raise ValueError('disk-app Image entry does not branch to its executable _start')
    result = {key: pool[key] for key in ('arch', 'address_space', 'image_start',
        'image_end', 'image_size', 'bss_start', 'bss_end', 'binary_sha256', 'raw_sha256')}
    result.update(objects=objects, page_size=PAGE, compiler_flags=flags,
                  image_entry_branch=branch, image_entry_target=target,
                  runtime_contract='fixed ARM Image; EL1h; storage MMU/cache off; I/O drained before own roots',
                  roots=2, table_pages_per_root=32, user_code_va=0x10000000,
                  user_stack_va=0x11000000, fault_va=0x12010000,
                  code_destination='zero-filled BSS; copied only after complete app validation',
                  instruction_proof='external payload only; boot compiler integer-only flags, no complete boot decode claim')
    return result


def validate_app_bytes(data):
    """Independently decode every byte of the closed T-P1.8B file grammar.

    Only the two MOVZ/MOVK marker immediates and MOVZ exit immediate vary.
    The header cannot provide an address, mapping, arbitrary entry or opcode.
    This is a complete integer proof of these 28 payload bytes, not a general
    AArch64 disassembler or a proof of the kernel's executable instructions.
    """
    if not isinstance(data, (bytes, bytearray)) or not APP_HEADER <= len(data) <= 1024:
        raise ValueError('disk-app file length is outside the fixed container limit')
    data = bytes(data)
    magic, version, isa, header, code_length, entry, reserved, checksum = \
        struct.unpack_from('<8sIHHIIII', data)
    if magic != APP_MAGIC or version != 1 or isa != 183 or header != APP_HEADER:
        raise ValueError('unsupported disk-app container header')
    if (code_length != APP_CODE_BYTES or code_length % 4 or entry != 0 or reserved or
            len(data) != header + code_length):
        raise ValueError('invalid disk-app code length, entry or reserved field')
    checked = data[:28] + bytes(4) + data[32:]
    if zlib.crc32(checked) & 0xffffffff != checksum:
        raise ValueError('disk-app container CRC mismatch')
    code = data[APP_HEADER:]
    words = struct.unpack('<7I', code)
    if (words[:3] != (0xb5000060, 0xd2a24021, 0xf9400022) or
            words[3] & 0xffe0001f != 0xd2800013 or
            words[4] & 0xffe0001f != 0xf2a00013 or
            words[5] & 0xffe0001f != 0xd2800000 or words[6] != 0xd4000041):
        raise ValueError('unsupported disk-app opcode or instruction order')
    marker = ((words[3] >> 5) & 0xffff) | (((words[4] >> 5) & 0xffff) << 16)
    exit_code = (words[5] >> 5) & 0xffff
    operations = ('cbnz x0, +12', 'movz x1, #0x1201, lsl #16', 'ldr x2, [x1]',
                  'movz x19, marker_low', 'movk x19, marker_high, lsl #16',
                  'movz x0, exit_code', 'svc #2')
    instructions = [dict(offset=index * 4, bytes=code[index*4:index*4+4].hex(),
                         instruction=operation)
                    for index, operation in enumerate(operations)]
    sites = {name: dict(offset=offset, user_pc=USER_CODE_VA + offset,
                       bytes=code[offset:offset+4].hex())
             for name, offset in (('entry', 0), ('fault', 8), ('normal', 12), ('exit', 24))}
    sites['exit']['saved_pc'] = USER_CODE_VA + APP_CODE_BYTES
    return dict(format='StyleOS T-P1.8B closed test container v1', isa='aarch64',
                file_bytes=len(data), file_hex=data.hex(),
                file_sha256=hashlib.sha256(data).hexdigest(), checksum=checksum,
                header_bytes=APP_HEADER, code_bytes=APP_CODE_BYTES, code_hex=code.hex(),
                code_sha256=hashlib.sha256(code).hexdigest(), entry_offset=entry,
                fault_offset=8, exit_offset=24, marker=marker, exit_code=exit_code,
                fault_va=FAULT_VA, user_code_va=USER_CODE_VA,
                sites=sites, instructions=instructions, integer_only=True)


def build_app_metadata(binary, source_path, raw_path=None):
    """Re-derive an external app's code and sites from its separate ELF.

    No builder JSON or kernel symbols supply the expected bytes. The one
    allocated .text section and its unique RX identity PT_LOAD must coincide
    exactly with the complete seven-instruction payload at linked address 0.
    """
    data = _read_file(binary, 'external disk-app ELF')
    if _range(data, 0, 16, 'app ELF identification')[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected ELF64 little-endian external app')
    h = _unpack('<HHIQQQIHHHHHH', data, 16, 'external app ELF header')
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, names_index = h
    if (kind != 2 or machine != 183 or version != 1 or entry != 0 or flags or
            ehsize != 64 or phsize != 56 or not 0 < phnum < 0xffff or
            shsize != 64 or not 0 < names_index < shnum):
        raise ValueError('unsupported external app ELF layout')
    _range(data, phoff, phnum * phsize, 'external app program headers')
    _range(data, shoff, shnum * shsize, 'external app section headers')
    sections = [_unpack('<IIQQQQIIQQ', data, shoff + i * shsize, 'app section')
                for i in range(shnum)]
    for section in sections:
        if section[3] + section[5] > 1 << 64:
            raise ValueError('external app section overflows')
        if section[1] != 8:
            _range(data, section[4], section[5], 'external app section contents')
    ns = sections[names_index]
    if ns[1] != 3:
        raise ValueError('app section names are not a string table')
    names = _range(data, ns[4], ns[5], 'external app section names')
    allocated = [section for section in sections if section[2] & 2 and section[5]]
    if len(allocated) != 1:
        raise ValueError('external app must have exactly one allocated code section')
    text = allocated[0]
    text_index = sections.index(text)
    if (text[1] != 1 or text[2] != 6 or text[3] != 0 or text[5] != APP_CODE_BYTES or
            _string(names, text[0], 'app section') != b'.text' or text[8] != 4):
        raise ValueError('external app .text is not the complete aligned RX payload')
    loads = [_unpack('<IIQQQQQQ', data, phoff + i * phsize, 'app LOAD')
             for i in range(phnum)]
    loads = [p for p in loads if p[0] == 1]
    if (len(loads) != 1 or loads[0][1] != 5 or loads[0][2] != text[4] or
            loads[0][3:7] != (0, 0, APP_CODE_BYTES, APP_CODE_BYTES) or
            loads[0][7] not in (4, 0x1000, 0x10000) or
            loads[0][2] % loads[0][7]):
        raise ValueError('external app has no unique complete identity RX LOAD')
    symtabs = [s for s in sections if s[1] == 2]
    if len(symtabs) != 1:
        raise ValueError('external app must have exactly one symbol table')
    table = symtabs[0]
    if table[9] != 24 or table[5] % 24 or not 0 < table[6] < shnum or sections[table[6]][1] != 3:
        raise ValueError('invalid external app symbol table')
    ss = sections[table[6]]
    strings = _range(data, ss[4], ss[5], 'external app symbol names')
    symbols = {}
    for off in range(table[4], table[4] + table[5], 24):
        name, info, other, index, value, size = _unpack('<IBBHQQ', data, off, 'app symbol')
        label = _string(strings, name, 'app symbol')
        if label.decode('ascii', errors='replace') in APP_SYMBOLS:
            symbols.setdefault(label.decode(), []).append((info, other, index, value, size))
    for label, address in APP_SYMBOLS.items():
        matches = symbols.get(label, [])
        if (len(matches) != 1 or matches[0][0] >> 4 != 1 or matches[0][0] & 15 not in (0, 2) or
                matches[0][1] & ~3 or matches[0][2] != text_index or
                matches[0][3] != address or matches[0][4] not in (0, APP_CODE_BYTES if address == 0 else 0)):
            raise ValueError(f'external app has invalid or duplicate site {label}')
    code = _range(data, text[4], text[5], 'external app code')
    if raw_path is not None and _read_file(raw_path, 'external app raw code') != code:
        raise ValueError('external app raw code disagrees with complete ELF payload')
    header = struct.pack('<8sIHHIIII', APP_MAGIC, 1, 183, APP_HEADER, len(code), 0, 0, 0)
    checksum = zlib.crc32(header + code) & 0xffffffff
    container = header[:28] + struct.pack('<I', checksum) + code
    result = validate_app_bytes(container)
    source = _read_file(source_path, 'external app source')
    result.update(binary_sha256=hashlib.sha256(data).hexdigest(),
                  source_sha256=hashlib.sha256(source).hexdigest(),
                  code_symbols=dict(APP_SYMBOLS))
    return result


def verify_table_closure(root, table_pages, entries, site, user=False):
    """Walk every descriptor in the finite SAFE/APP root and require exact leaves.

    `table_pages` lists all 32 reserved pages for this root; `entries` supplies
    every page as 4096 bytes or 512 integer descriptors, including implicit
    zero entries reconstructed from the complete sparse guest dump. All
    unused pages must be zero. Blocks, aliases, foreign tables and attributes,
    duplicate table references and extra user/MMIO mappings are refused.
    """
    obj = site['objects']
    reserve = obj['tables']['start'] + (32 * PAGE if user else 0)
    expected_pages = list(range(reserve, reserve + 32 * PAGE, PAGE))
    if (list(table_pages) != expected_pages or root != reserve or
            set(entries) != set(expected_pages)):
        raise ValueError('finite disk-app root/page reserve is incomplete or foreign')
    pages = {}
    for pa in expected_pages:
        page = entries[pa]
        if isinstance(page, (bytes, bytearray)):
            if len(page) != PAGE:
                raise ValueError('incomplete raw disk-app table page')
            values = struct.unpack('<512Q', page)
        else:
            try:
                values = tuple(page)
            except TypeError as error:
                raise ValueError('invalid disk-app table page') from error
            if (len(values) != 512 or any(type(v) is not int or not 0 <= v < 1 << 64 for v in values)):
                raise ValueError('invalid disk-app table descriptor extent')
        pages[pa] = values
    image_start = site['image_start'] & -PAGE
    image_end = (site['image_end'] + PAGE - 1) & -PAGE
    expected = {pa: pa | 0x40000000000703 for pa in range(image_start, image_end, PAGE)}
    expected[0x09000000] = 0x09000000 | 0x60000000000607
    if user:
        expected[USER_CODE_VA] = obj['code']['start'] | 0x200000000007c3
        expected[USER_STACK_VA] = obj['user_stack']['start'] | 0x60000000000743
    visited, leaves = set(), {}

    def walk(pa, level, prefix):
        if pa in visited or pa not in pages:
            raise ValueError('foreign, cyclic or shared disk-app table reference')
        visited.add(pa)
        for index, descriptor in enumerate(pages[pa]):
            if descriptor == 0:
                continue
            va = prefix | (index << (39 - level * 9))
            if level < 3:
                child = descriptor & 0x0000fffffffff000
                if descriptor != child | 3:
                    raise ValueError('disk-app table contains block or unsupported table attributes')
                walk(child, level + 1, va)
            else:
                if va in leaves or expected.get(va) != descriptor:
                    raise ValueError('disk-app leaf has foreign VA/PA, rights or memory type')
                leaves[va] = descriptor

    walk(root, 0, 0)
    if leaves != expected:
        raise ValueError('disk-app table misses complete image/UART/user mapping closure')
    if any(any(pages[pa]) for pa in expected_pages if pa not in visited):
        raise ValueError('unreachable disk-app reserve contains live descriptors')
    raw = b''.join(struct.pack('<512Q', *pages[pa]) for pa in expected_pages)
    leaf_raw = b''.join(struct.pack('<QQ', va, descriptor) for va, descriptor in sorted(leaves.items()))
    return dict(root=root, reserved_pages=32, used_pages=len(visited),
                descriptor_count=sum(sum(value != 0 for value in page) for page in pages.values()),
                leaf_count=len(leaves), user_leaf_count=2 if user else 0,
                table_sha256=hashlib.sha256(raw).hexdigest(),
                leaf_sha256=hashlib.sha256(leaf_raw).hexdigest(),
                virtio_mmio_mapped=False, fault_va_mapped=FAULT_VA in leaves)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--raw', type=Path, required=True)
    parser.add_argument('--compiler-flags', required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    text = json.dumps(describe_disk_app('aarch64', args.binary,
        raw_path=args.raw, compiler_flags=args.compiler_flags), indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
