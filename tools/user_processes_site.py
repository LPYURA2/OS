#!/usr/bin/env python3
"""Prove image-owned process resources and exact linked user instructions.

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
    from .integer_code import verify_integer_code
except ImportError:
    from fault_site import _range, _unpack, _string, _read_file
    from pool_site import describe_pool, _interval, _inside, _intersects
    from integer_code import verify_integer_code

PAGE = 4096
DIAGNOSTIC_BYTES = 12400
STACK_BYTES = 65536
FORBIDDEN = frozenset(('styleos_paging_tables', 'styleos_paging_control',
                       'styleos_paging_data', 'styleos_test_fault',
                       'styleos_paging_fault', 'styleos_ownership_workspace',
                       'styleos_tasks_stack_a', 'styleos_tasks_stack_b',
                       'styleos_tasks_control', 'tasks_arch_switch'))
COMMON = {
    'tables': ('styleos_user_tables', 3 * 32 * PAGE, PAGE),
    'control': ('styleos_user_control', 16384, PAGE),
    'entry_stack': ('styleos_user_entry_stack', 16384, PAGE),
    'stack_a': ('styleos_user_stack_a', PAGE, PAGE),
    'stack_b': ('styleos_user_stack_b', PAGE, PAGE),
    'data_a': ('styleos_user_data_a', PAGE, PAGE),
    'data_b': ('styleos_user_data_b', PAGE, PAGE),
    'diagnostic': ('diagnostic', DIAGNOSTIC_BYTES, 8),
}
X86_OBJECTS = {
    'gdt': ('styleos_user_x86_gdt', PAGE, PAGE),
    'tss': ('styleos_user_x86_tss', PAGE, PAGE),
    'user_idt': ('styleos_user_x86_idt', PAGE, PAGE),
}
STATIC_ORIGINS = {'diagnostic': 'kernel_boot_memory_diagnostic.o',
                  'handoff': 'boot_x86_64_uefi.o',
                  'early_idt': 'arch_x86_64_exceptions.o'}
CODE_SYMBOLS = ('styleos_user_a_start', 'styleos_user_a_yield_resume',
                'styleos_user_a_fault', 'styleos_user_b_start',
                'styleos_user_b_yield_resume', 'styleos_user_b_exit')
CODE_PAGES = {'code_a': ('styleos_user_code_a', 'styleos_user_code_a_end'),
              'code_b': ('styleos_user_code_b', 'styleos_user_code_b_end')}
USER_CODE_VA = 0x10000000
USER_STACK_VA = {'A': 0x11000000, 'B': 0x11010000}
USER_DATA_VA = {'A': 0x12000000, 'B': 0x12010000}
FAULT_READ = {'x86_64': bytes.fromhex('488b07'),
              'aarch64': bytes.fromhex('010040f9')}


def _item(symbol, bounds):
    return dict(symbol=symbol, start=bounds[0], end=bounds[1],
                bytes=bounds[1] - bounds[0])


def _finish(pool, objects):
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
    result.update(objects=objects, page_size=PAGE)
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
            raise ValueError('unrelated profile buffer or fault present in user-process image')
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
        types = (0, 1, 2) if code else (0,) if endpoint else (1,)
        index, address, declared = symbol(name, types, local)
        bounds = _interval(address, size, name)
        section = sections[index]
        if address % alignment:
            raise ValueError(f'{name} is not correctly aligned')
        if endpoint:
            end_index, end, end_size = symbol(endpoint, (0,))
            if (end != bounds[1] or end_index != index or end_size or
                    declared not in ((0, size) if code else (0,))):
                raise ValueError('object boundaries do not describe the complete expected extent')
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
    objects['user_vectors'] = resolve('styleos_user_vectors', 2048, 2048, code=True)
    for key, (name, endpoint) in CODE_PAGES.items():
        objects[key] = resolve(name, PAGE, PAGE, code=True, endpoint=endpoint)

    spans = []
    for s in sections:
        if not s[2] & 4 or not s[5]:
            continue
        if s[1] != 1 or s[2] & 6 != 6:
            raise ValueError('executable ELF section is not allocated PROGBITS')
        bounds = _interval(s[3], s[5], 'executable ELF section')
        mappings = [p for p in loads if _intersects(bounds, (p[3], p[3] + p[6]))]
        if (not _inside(bounds, image) or len(mappings) != 1 or
                mappings[0][1] & 5 != 5 or mappings[0][3] != mappings[0][4] or
                not _inside(bounds, (mappings[0][3], mappings[0][3] + mappings[0][5])) or
                mappings[0][2] + s[3] - mappings[0][3] != s[4]):
            raise ValueError('executable ELF section has no complete identity PT_LOAD')
        section_name = _string(names, s[0], 'section').decode('ascii')
        # The Linux ARM64 Image header contains 60 bytes of declared header
        # data following the entry branch. Decode the branch and every byte
        # following that header; do not misinterpret the header as instructions.
        pieces = [(s[3], s[3] + s[5])]
        if s[3] <= image[0] < s[3] + s[5]:
            if s[3] != image[0] or s[5] < 64:
                raise ValueError('ARM Image header is not at executable section start')
            entry = _unpack('<I', data, s[4], 'ARM Image entry branch')[0]
            immediate = entry & 0x3ffffff
            if immediate & 0x2000000:
                immediate -= 0x4000000
            target = image[0] + immediate * 4
            if entry & 0xfc000000 != 0x14000000 or not image[0] + 64 <= target < s[3] + s[5]:
                raise ValueError('ARM Image entry does not branch beyond its declared header data')
            pieces = [(image[0], image[0] + 4), (image[0] + 64, s[3] + s[5])]
        for start, end in pieces:
            if start < end:
                spans.append(dict(start=start, end=end,
                                  offset=s[4] + start - s[3], section=section_name))

    def code_symbol(name):
        index, address, size = symbol(name, (0, 2))
        section = sections[index]
        if (section[1] != 1 or section[2] & 6 != 6 or address % 4 or
                not section[3] <= address < section[3] + section[5]):
            raise ValueError(f'{name} is not an aligned executable ELF symbol')
        return dict(symbol=name, start=address, declared_bytes=0,
                    raw_offset=section[4] + address - section[3])

    code = {name: code_symbol(name) for name in CODE_SYMBOLS}
    return objects, spans, code


def _complete_code(arch, data, pool, spans, code, decoded):
    """Bind named user sites to actual decoded instruction boundaries."""
    native_base = pool.get('preferred_image_base', 0)
    result = {}
    for name, row in code.items():
        native = native_base + row['start']
        instruction = decoded.get(native)
        if instruction is None or instruction.get('kind', 'instruction') != 'instruction':
            raise ValueError(f'{name} does not name a decoded instruction boundary')
        chunk = bytes.fromhex(instruction['bytes'])
        if not chunk:
            raise ValueError(f'{name} names an empty decoded instruction')
        bounds = _interval(native, len(chunk), name)
        if not any(_inside(bounds, (span['start'], span['end'])) for span in spans):
            raise ValueError(f'{name} extends outside executable bytes')
        actual = _range(data, row['raw_offset'], len(chunk), f'{name} linked bytes')
        if actual != chunk:
            raise ValueError(f'{name} raw bytes disagree with the complete decode')
        item = _item(name, (row['start'], row['start'] + len(chunk)))
        item.update(raw_offset=row['raw_offset'], raw_hex=chunk.hex(),
                    instruction_hex=chunk.hex(), sha256=hashlib.sha256(chunk).hexdigest(),
                    instructions=[dict(offset=0, bytes=chunk.hex(),
                                       instruction=instruction['instruction'])])
        item['pc' if arch == 'aarch64' else 'rva'] = row['start']
        result[name] = item
    return result


def _programs(arch, site, decoded):
    """Derive trap/fault user PCs from actual linked page bytes, never UART."""
    base = site.get('preferred_image_base', 0)
    result = {}
    trap_bytes = 2 if arch == 'x86_64' else 4
    yield_raw = bytes.fromhex('cd80') if arch == 'x86_64' else (0xd4000021).to_bytes(4, 'little')
    exit_raw = bytes.fromhex('cd80') if arch == 'x86_64' else (0xd4000041).to_bytes(4, 'little')
    for ident in ('A', 'B'):
        suffix = ident.lower()
        page = site['objects']['code_' + suffix]
        image_bounds = (page['start'], page['end'])
        native_bounds = (base + page['start'], base + page['end'])
        raw = bytearray()
        pc = native_bounds[0]
        while pc < native_bounds[1]:
            instruction = decoded.get(pc)
            if instruction is None or instruction.get('kind', 'instruction') != 'instruction':
                raise ValueError('user code page has a gap or declared non-instruction data')
            chunk = bytes.fromhex(instruction['bytes'])
            if not chunk or pc + len(chunk) > native_bounds[1]:
                raise ValueError('user code page ends inside a decoded instruction')
            raw.extend(chunk)
            pc += len(chunk)
        if len(raw) != PAGE:
            raise ValueError('user program is not one complete decoded page')

        def location(image_pc, symbol):
            native_pc = base + image_pc
            instruction = decoded.get(native_pc)
            if (not image_bounds[0] <= image_pc < image_bounds[1] or instruction is None or
                    instruction.get('kind', 'instruction') != 'instruction'):
                raise ValueError(f'{symbol} is not an instruction in its own user code page')
            chunk = bytes.fromhex(instruction['bytes'])
            if image_pc + len(chunk) > image_bounds[1]:
                raise ValueError('user instruction crosses code page end')
            key = 'image_pc' if arch == 'aarch64' else 'image_rva'
            return dict(symbol=symbol, user_pc=USER_CODE_VA + image_pc - page['start'],
                        **{key: image_pc}, raw_hex=chunk.hex(),
                        sha256=hashlib.sha256(chunk).hexdigest())

        def linked(name):
            return location(site['code_sites'][name]['start'], name)

        entry = linked('styleos_user_' + suffix + '_start')
        if entry['user_pc'] != USER_CODE_VA:
            raise ValueError('user entry does not start at its code page')
        resume = linked('styleos_user_' + suffix + '_yield_resume')
        call_image = page['start'] + resume['user_pc'] - USER_CODE_VA - trap_bytes
        call = location(call_image, 'styleos_user_' + suffix + '_yield')
        if bytes.fromhex(call['raw_hex']) != yield_raw:
            raise ValueError('user yield continuation does not follow the expected hardware trap')
        resume['call_site'] = call
        sites = dict(entry=entry, yield_resume=resume)
        if ident == 'A':
            fault = linked('styleos_user_a_fault')
            if bytes.fromhex(fault['raw_hex']) != FAULT_READ[arch]:
                raise ValueError('user fault symbol is not the exact expected memory read')
            if fault['user_pc'] <= resume['user_pc']:
                raise ValueError('user fault read does not follow its saved yield continuation')
            sites['fault'] = fault
        else:
            exit_site = linked('styleos_user_b_exit')
            if bytes.fromhex(exit_site['raw_hex']) != exit_raw:
                raise ValueError('user B exit symbol is not the expected hardware trap')
            if exit_site['user_pc'] <= resume['user_pc']:
                raise ValueError('user B exit does not follow its saved yield continuation')
            exit_site['saved_pc'] = exit_site['user_pc'] + trap_bytes
            sites['exit'] = exit_site
            sites['exit_resume'] = location(
                page['start'] + exit_site['saved_pc'] - USER_CODE_VA,
                'styleos_user_b_exit_resume')
        result[ident] = dict(code_va=USER_CODE_VA, code_bytes=PAGE,
                             stack_va=USER_STACK_VA[ident], stack_bytes=PAGE,
                             data_va=USER_DATA_VA[ident], data_bytes=PAGE,
                             code_object='code_' + suffix,
                             stack_object='stack_' + suffix,
                             data_object='data_' + suffix,
                             raw_hex=raw.hex(), sha256=hashlib.sha256(raw).hexdigest(),
                             sites=sites)
    if _intersects((USER_STACK_VA['A'], USER_STACK_VA['A'] + PAGE),
                   (USER_STACK_VA['B'], USER_STACK_VA['B'] + PAGE)):
        raise ValueError('user stacks share a virtual page')
    return result


def describe_user_processes(arch, binary, map_path=None, raw_path=None, compiler_flags=None):
    """Return full process resources, user VA sites and decoded integer evidence.

    A PE start/end remains an RVA until combined with the observed load base.
    ARM addresses apply to this identity-addressed QEMU Image only. Compiler
    flags are necessary supporting evidence; actual linked executable bytes
    are independently decoded, with no omitted or undecodable instructions.
    The decoder dependency is confined to profiles that request linked integer proof.
    """
    try:
        pool = describe_pool(arch, binary, map_path, raw_path)
        data = _read_file(binary, 'binary')
        if hashlib.sha256(data).hexdigest() != pool['binary_sha256']:
            raise ValueError('binary changed during user-process proof')
        if arch == 'aarch64':
            raw = _read_file(raw_path, 'raw Image')
            if hashlib.sha256(raw).hexdigest() != pool['raw_sha256']:
                raise ValueError('raw Image changed during user-process proof')
            objects, spans, code = _elf_objects(data, pool)
        else:
            map_bytes = _read_file(map_path, 'linker map')
            if hashlib.sha256(map_bytes).hexdigest() != pool['map_sha256']:
                raise ValueError('linker map changed during user-process proof')
            objects, spans, code = _pe_objects(data, map_bytes, pool)
        result = _finish(pool, objects)
        native_base = pool.get('preferred_image_base', 0)
        for name, item in result['objects'].items():
            if name in ('vectors', 'user_vectors', 'code_a', 'code_b'):
                continue
            bounds = (native_base + item['start'], native_base + item['end'])
            if any(_intersects(bounds, (s['start'], s['end'])) for s in spans):
                raise ValueError(f'{name} storage overlaps executable image bytes')
        proof, decoded = verify_integer_code(arch, binary, spans, compiler_flags)
        if hashlib.sha256(_read_file(binary, 'binary')).hexdigest() != pool['binary_sha256']:
            raise ValueError('binary changed during integer-code proof')
        result['code_sites'] = _complete_code(arch, data, pool, spans, code, decoded)
        result['integer_only'] = proof
        result['programs'] = _programs(arch, result, decoded)
        result['table_pages_per_root'] = 32
        result['roots'] = 3
        result['fault_va'] = USER_DATA_VA['B']
        return result
    except (OSError, UnicodeError, struct.error) as error:
        raise ValueError(f'cannot describe user-process site: {error}') from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=('aarch64', 'x86_64'), required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--map', dest='map_path', type=Path)
    parser.add_argument('--raw', dest='raw_path', type=Path)
    parser.add_argument('--compiler-flags', required=True,
                        help='actual compiler flags as a shell-quoted string')
    args = parser.parse_args()
    try:
        result = describe_user_processes(args.arch, args.binary, args.map_path,
                                args.raw_path, args.compiler_flags)
    except ValueError as error:
        parser.exit(1, f'user-processes-site error: {error}\n')
    print(json.dumps(result, sort_keys=True, indent=2))


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
            raise ValueError('unrelated profile buffer or fault present in user-process image')
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

    def resolve_code_page(name, endpoint):
        segment, relative, address = symbol(name)
        end_segment, end_relative, end = symbol(endpoint)
        bounds = _interval(address, PAGE, name, 1 << 32)
        if (address % PAGE or end != bounds[1] or end_segment != segment or
                end_relative != relative + PAGE or end > image_size):
            raise ValueError(f'{name} is not one complete aligned user code page')
        matches = [(n, s) for n, s in enumerate(sections, 1)
                   if _intersects(bounds, (s[2], s[2] + (s[1] or s[3])))]
        if len(matches) != 1:
            raise ValueError(f'{name} has no unique executable PE section')
        number, section = matches[0]
        if (segment != number or relative != address - section[2] or
                not section[9] & 0x20000000 or not section[9] & 0x20 or
                not _inside(bounds, (section[2], section[2] + section[1])) or
                relative + PAGE > section[3]):
            raise ValueError(f'{name} is not completely file-backed executable PE code')
        return _item(name, bounds)

    objects = {key: resolve(name, size, alignment)
               for key, (name, size, alignment) in {**COMMON, **X86_OBJECTS}.items()}
    objects['stack'] = resolve('styleos_stack_bottom', STACK_BYTES, 16,
                               endpoint='styleos_stack_top')
    objects['idt'] = resolve('early_idt', 4096, 16)
    objects['handoff'] = resolve('handoff', 96, 8, initialized=True)
    for key, (name, endpoint) in CODE_PAGES.items():
        objects[key] = resolve_code_page(name, endpoint)

    spans = []
    for s in sections:
        if not s[9] & 0x20000000:
            continue
        size = s[1] or s[3]
        if not size or size > s[3] or not s[9] & 0x20:
            raise ValueError('executable PE section is not wholly file-backed code')
        spans.append(dict(start=base + s[2], end=base + s[2] + size,
                          offset=s[4], section=s[0].split(b'\0', 1)[0].decode('ascii')))

    def code_symbol(name):
        segment, relative, address = symbol(name)
        matches = [(number, s) for number, s in enumerate(sections, 1)
                   if s[2] <= address < s[2] + (s[1] or s[3])]
        if len(matches) != 1:
            raise ValueError(f'{name} has no unique executable PE section')
        number, s = matches[0]
        if (segment != number or relative != address - s[2] or
                not s[9] & 0x20000000 or not s[9] & 0x20 or relative >= s[3]):
            raise ValueError(f'{name} is not in matching file-backed PE code')
        return dict(symbol=name, start=address, declared_bytes=0,
                    raw_offset=s[4] + relative)

    code = {name: code_symbol(name) for name in CODE_SYMBOLS}
    return objects, spans, code


if __name__ == '__main__':
    main()


