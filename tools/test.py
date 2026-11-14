#!/usr/bin/env python3
"""Host, reproducible-build and bounded real-QEMU checks for T-P1.4."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import re
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import time

try:
    from .pool_site import describe_pool
except ImportError:  # Direct tools/test.py invocation.
    from pool_site import describe_pool

try:
    from .paging_site import describe_paging
except ImportError:  # Direct tools/test.py invocation.
    from paging_site import describe_paging

ROOT = Path(__file__).resolve().parents[1]


def serial_handoff_ok(output, arch, invalid=False, el2=False):
    """A successful marker cannot erase a prior boot failure in the same run."""
    errors = (b'BOOT ERROR', b'BOOT_ERROR', b'early exception',
              b'STYLEOS EXCEPTION', b'STYLEOS EMERGENCY', b'STYLEOS BOOTPOOL',
              b'STYLEOS PAGING')
    if any(marker in output for marker in errors):
        return False
    lines = output.splitlines()
    if invalid:
        return (lines.count(b'STYLEOS BOOT_INFO ERROR: magic') == 1 and
                b'STYLEOS HANDOFF PASS' not in output and
                b'STYLEOS MEMMAP' not in output and b'STYLEOS KERNEL HALT' not in output)
    if (lines.count(('STYLEOS HANDOFF PASS ' + arch).encode()) != 1 or
            b'BOOT_INFO ERROR' in output):
        return False
    if arch == 'aarch64':
        expected_el = b'STYLEOS AARCH64: initial EL2' if el2 else b'STYLEOS AARCH64: initial EL1'
        return (lines.count(expected_el) == 1 and
                lines.count(b'STYLEOS AARCH64: current EL1') == 1 and
                not (b'initial EL1' in output and b'initial EL2' in output))
    return lines.count(b'STYLEOS x86_64 ExitBootServices OK') == 1


_RANGE = re.compile(rb'STYLEOS MEMMAP (IMAGE|STACK|HANDOFF|SOURCE|RAM|RESERVED|CANDIDATE)'
                    rb' 0x([0-9a-f]{16}) 0x([0-9a-f]{16})')


def serial_memory_ok(output, arch, mode, ram_mib=128, el2=False, terminal=True):
    """Check UART order and independently reconstruct the diagnostic ranges."""
    if not serial_handoff_ok(output, arch, mode == 'invalid', el2):
        return False
    if mode == 'invalid':
        return True
    lines = output.splitlines()
    handoff = ('STYLEOS HANDOFF PASS ' + arch).encode()
    halt, passed = b'STYLEOS KERNEL HALT', b'STYLEOS MEMMAP PASS'
    diagnostic = b'STYLEOS MEMMAP DIAGNOSTIC ONLY'
    mmap = [line for line in lines if line.startswith(b'STYLEOS MEMMAP ')]
    if mode == 'invalid-map':
        reason = b'uefi-format' if arch == 'x86_64' else b'fdt-format'
        err = b'STYLEOS MEMMAP ERROR: ' + reason
        return mmap == [err] and halt not in lines and lines.index(handoff) < lines.index(err)
    if (not mmap or mmap[0] != diagnostic or mmap[-1] != passed or
            b'MEMMAP ERROR' in output or
            not lines.index(handoff) < lines.index(diagnostic) < lines.index(passed)):
        return False
    if terminal and (lines.count(halt) != 1 or lines.index(passed) >= lines.index(halt)):
        return False
    if not terminal and halt in lines:
        return False
    kinds = (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE', b'RAM', b'RESERVED', b'CANDIDATE')
    by_kind = {kind: [] for kind in kinds}
    for line in mmap[1:-1]:
        match = _RANGE.fullmatch(line)
        if not match:
            return False
        kind, a, b = match.groups()
        a, b = int(a, 16), int(b, 16)
        if a >= b:
            return False
        by_kind[kind].append((a, b))
    if any(len(by_kind[k]) != 1 for k in kinds[:4]):
        return False
    image, stack, info, source = (by_kind[k][0] for k in kinds[:4])
    ram, reserved, candidates = (by_kind[k] for k in kinds[4:])
    if not ram or not reserved or not candidates:
        return False
    def includes(ranges, inner):
        return any(a <= inner[0] and b >= inner[1] for a, b in ranges)
    if (not includes([image], stack) or not includes([image], info) or
            stack[1] - stack[0] != 65536 or info[1] - info[0] != 96 or
            any(not includes(reserved, r) for r in (image, stack, info, source))):
        return False
    for ranges in (ram, reserved, candidates):
        if any(b > c for (_, b), (c, _) in zip(ranges, ranges[1:])):
            return False
    if any((a | b) & 4095 for a, b in reserved + candidates):
        return False
    if arch == 'aarch64':
        if (sum(b - a for a, b in ram) != ram_mib * (1 << 20) or
                not includes(ram, source) or not includes(ram, image) or
                not includes(reserved, (0x9000000, 0x9001000))):
            return False
    # Interval sweep independent of the guest implementation. Start/end
    # event totals expose missing or extra candidate pages as well as overlaps.
    events = {}
    for group, ranges in enumerate((ram, reserved, candidates)):
        for a, b in ranges:
            if group == 0:
                a, b = (a + 4095) & ~4095, b & ~4095
                if a >= b:
                    continue
            events.setdefault(a, [0, 0, 0])[group] += 1
            events.setdefault(b, [0, 0, 0])[group] -= 1
    active = [0, 0, 0]
    for address in sorted(events):
        active = [a + delta for a, delta in zip(active, events[address])]
        if active[2] != int(active[0] == 1 and active[1] == 0):
            return False
    return True


def serial_exception_ok(output, arch, site, ram_mib=128, el2=False):
    """Check a fault against the built instruction, including PE relocation.

    The prefix check consumes actual UART lines without synthesizing a HALT.
    """
    lines = output.splitlines()
    records = [line for line in lines if line.startswith(b'STYLEOS EXCEPTION arch=')]
    emergency = b'STYLEOS EMERGENCY HALT'
    if (len(records) != 1 or lines.count(emergency) != 1 or
            b'STYLEOS KERNEL HALT' in output or site.get('arch') != arch or
            site.get('symbol') != 'styleos_test_fault'):
        return False
    first = next((i for i, line in enumerate(lines)
                  if line.startswith(b'STYLEOS EXCEPTION')), None)
    if first is None:
        return False
    prefix = b'\n'.join(lines[:first]) + b'\n'
    if not serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False):
        return False
    style_tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    images = [_RANGE.fullmatch(line) for line in prefix.splitlines()]
    images = [match for match in images if match and match[1] == b'IMAGE']
    if len(images) != 1:
        return False
    image_base, image_end = int(images[0][2], 16), int(images[0][3], 16)
    if arch == 'x86_64':
        match = re.fullmatch(rb'STYLEOS EXCEPTION arch=x86_64 kind=sync vector=6 '
                             rb'reason=invalid-opcode pc=(0x[0-9a-f]{16}) '
                             rb'error=absent cr2=absent', records[0])
        tests = [line for line in lines if line.startswith(b'STYLEOS EXCEPTION TEST')]
        if not match or len(tests) != 1 or site.get('instruction_hex') != '0f0b':
            return False
        probe = re.fullmatch(rb'STYLEOS EXCEPTION TEST x86_64 pc=(0x[0-9a-f]{16})', tests[0])
        rva = site.get('rva')
        if not probe or not isinstance(rva, int) or rva < 0:
            return False
        expected = image_base + rva
        return (image_base <= expected and expected + 2 <= image_end and
                int(match[1], 16) == expected == int(probe[1], 16) and
                style_tail == [tests[0], records[0], emergency])
    if arch != 'aarch64' or site.get('instruction_hex') != '002420d4':
        return False
    match = re.fullmatch(rb'STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x3c '
                         rb'reason=brk pc=(0x[0-9a-f]{16}) esr=(0x[0-9a-f]{16}) '
                         rb'elr=(0x[0-9a-f]{16}) spsr=(0x[0-9a-f]{16}) far=absent', records[0])
    if not match:
        return False
    pc, esr, elr, spsr = (int(value, 16) for value in match.groups())
    return (pc == elr == site.get('pc') and image_base == site.get('image_base') and
            image_base <= pc and pc + 4 <= image_end and pc % 4 == 0 and
            (esr >> 26) & 63 == 0x3c and esr & 0x1ffffff == 0x120 and
            esr & (1 << 25) != 0 and spsr & 0x3df == 0x3c5 and
            style_tail == [records[0], emergency])


_POOL_RANGE = re.compile(rb'STYLEOS BOOTPOOL (RANGE|META|WORKSPACE)'
                         rb' 0x([0-9a-f]{16}) 0x([0-9a-f]{16})')


def serial_pool_ok(output, arch, site, ram_mib=128, el2=False):
    """Prove complete admitted spans and the exact selftest record sequence.

    The caller binds site to the actual ELF/raw Image or PE/map/disk. PE RVAs
    are relocated using the live MEMMAP IMAGE base, never its preferred base.
    The unmodified UART prefix must independently pass the memory validator.
    """
    if (not isinstance(site, dict) or site.get('arch') != arch or
            b'BOOTPOOL ERROR' in output or b'timeout' in output.lower() or
            b'timed out' in output.lower()):
        return False
    integer_keys = ('pool_start', 'pool_end', 'metadata_start', 'metadata_end',
                    'pool_bytes', 'metadata_bytes', 'page_size', 'image_start',
                    'image_end', 'image_size')
    if any(type(site.get(key)) is not int or not 0 <= site[key] < 1 << 64
           for key in integer_keys):
        return False
    if (site['pool_bytes'] != 32768 or site['metadata_bytes'] != 4096 or
            site['page_size'] != 4096 or
            site['pool_end'] - site['pool_start'] != 32768 or
            site['metadata_end'] - site['metadata_start'] != 4096):
        return False

    lines = output.splitlines()
    first = next((i for i, line in enumerate(lines)
                  if line.startswith(b'STYLEOS BOOTPOOL')), None)
    if first is None:
        return False
    prefix = b'\n'.join(lines[:first]) + b'\n'
    if not serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False):
        return False
    memory = {}
    for line in prefix.splitlines():
        match = _RANGE.fullmatch(line)
        if match:
            memory.setdefault(match[1], []).append((int(match[2], 16), int(match[3], 16)))
    image, stack, handoff, source = (memory[key][0]
                                   for key in (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE'))

    # The very first pool records declare all exclusion-relevant intervals.
    tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    if len(tail) < 3:
        return False
    declared = []
    for name, line in zip((b'RANGE', b'META', b'WORKSPACE'), tail[:3]):
        match = _POOL_RANGE.fullmatch(line)
        if not match or match[1] != name:
            return False
        bounds = int(match[2], 16), int(match[3], 16)
        if bounds[0] >= bounds[1]:
            return False
        declared.append(bounds)
    pool, metadata, workspace = declared

    def inside(inner, outer):
        return outer[0] <= inner[0] < inner[1] <= outer[1]

    def overlaps(one, two):
        return one[0] < two[1] and two[0] < one[1]

    if (pool[1] - pool[0] != 32768 or metadata[1] - metadata[0] != 4096 or
            any((start | end) & 4095 for start, end in (pool, metadata)) or
            overlaps(pool, metadata) or
            any(not inside(bounds, image) or
                not any(inside(bounds, reserve) for reserve in memory[b'RESERVED'])
                for bounds in (pool, metadata, workspace)) or
            any(overlaps(bounds, excluded) for bounds in (pool, metadata)
                for excluded in (stack, handoff, source, workspace))):
        return False

    if arch == 'x86_64':
        rva_keys = ('pool_rva_start', 'pool_rva_end', 'metadata_rva_start',
                    'metadata_rva_end', 'preferred_image_base')
        if (site.get('address_space') != 'image-rva' or
                any(type(site.get(key)) is not int or not 0 <= site[key] < 1 << 64
                    for key in rva_keys) or
                tuple(site[key] for key in rva_keys[:4]) !=
                tuple(site[key] for key in integer_keys[:4]) or
                site['image_start'] != 0 or site['image_end'] != site['image_size'] or
                site['image_size'] != image[1] - image[0]):
            return False
        expected_pool = image[0] + site['pool_start'], image[0] + site['pool_end']
        expected_metadata = (image[0] + site['metadata_start'],
                             image[0] + site['metadata_end'])
    elif arch == 'aarch64':
        if (site.get('address_space') != 'identity-image-address' or
                any(type(site.get(key)) is not int or not 0 <= site[key] < 1 << 64
                    for key in ('bss_start', 'bss_end')) or
                image != (site['image_start'], site['image_end']) or
                site['image_size'] != image[1] - image[0] or
                not inside((site['bss_start'], site['bss_end']), image) or
                any(not inside(bounds, (site['bss_start'], site['bss_end']))
                    for bounds in (pool, metadata))):
            return False
        expected_pool = site['pool_start'], site['pool_end']
        expected_metadata = site['metadata_start'], site['metadata_end']
    else:
        return False
    if pool != expected_pool or metadata != expected_metadata:
        return False

    # Eight aligned, distinct pages spanning this exact pool leave no freedom
    # to conceal a page outside the verified interval. Core uses first-free
    # order, and the test frees/reissues exactly allocation index 3.
    expected = list(tail[:3]) + [b'STYLEOS BOOTPOOL ADMIT PASS']
    expected.extend(f'STYLEOS BOOTPOOL ALLOC {index} 0x{pool[0] + index * 4096:016x} ZERO'.encode()
                    for index in range(8))
    expected += [b'STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096',
                 b'STYLEOS BOOTPOOL EXHAUSTED PASS',
                 f'STYLEOS BOOTPOOL REUSE 0x{pool[0] + 3 * 4096:016x} ZERO'.encode(),
                 b'STYLEOS BOOTPOOL PASS', b'STYLEOS KERNEL HALT']
    return tail == expected


def verified_pool_site(folder, arch):
    """Rebuild the static proof and bind it to every input actually booted."""
    target = folder / arch
    site = json.loads((target / 'pool-site.json').read_text())
    if arch == 'x86_64':
        verified = describe_pool(arch, target / 'BOOTX64.EFI', target / 'kernel.map')
        verified['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
    else:
        verified = describe_pool(arch, target / 'kernel.elf', raw_path=target / 'styleos.bin')
    if site != verified:
        raise ValueError('pool-site metadata differs from independently checked image proof/SHA-256')
    return verified


_HEX16 = rb'0x([0-9a-f]{16})'
_PAGING_RANGE = re.compile(rb'STYLEOS PAGING (TABLES|CONTROL|DATA|WORKSPACE) ' +
                           _HEX16 + rb' ' + _HEX16)
_PAGING_MAP = re.compile(rb'STYLEOS PAGING (MAP|PTE) va=' + _HEX16 +
                         rb' pa=' + _HEX16 + rb'(?: bytes=' + _HEX16 + rb')? flags=' + _HEX16)
_PAGING_ALIAS = 0x400000000000
_PAGING_PROBE = _PAGING_ALIAS + 4096


def _paging_fields(line, label, names):
    pattern = label + b''.join(b' ' + name.encode() + b'=' + _HEX16 for name in names)
    match = re.fullmatch(pattern, line)
    return dict(zip(names, (int(value, 16) for value in match.groups()))) if match else None


def serial_paging_ok(output, arch, site, fault=False, ram_mib=128, el2=False):
    """Validate a new-profile transition, every live leaf and its terminal event.

    The immutable binary proof is rederived by verified_paging_site(). The UART
    supplies actual hardware registers and actual relocated image placement;
    neither the preferred PE base nor a lone register/PASS marker is evidence.
    """
    if (not isinstance(site, dict) or site.get('arch') != arch or
            type(fault) is not bool or site.get('fault') is not fault or
            b'PAGING ERROR' in output or b'BOOT_ERROR' in output or
            b'BOOT ERROR' in output or b'BOOT_INFO ERROR' in output or
            b'timeout' in output.lower() or b'timed out' in output.lower()):
        return False
    integers = ('image_start', 'image_end', 'image_size', 'page_size',
                'table_pages', 'alias', 'probe')
    if (any(type(site.get(key)) is not int or not 0 <= site[key] < 1 << 64
            for key in integers) or site['page_size'] != 4096 or
            site['table_pages'] != 32 or site['alias'] != _PAGING_ALIAS or
            site['probe'] != _PAGING_PROBE):
        return False
    lines = output.splitlines()
    first = next((i for i, line in enumerate(lines)
                  if line.startswith(b'STYLEOS PAGING')), None)
    if first is None:
        return False
    prefix = b'\n'.join(lines[:first]) + b'\n'
    if not serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False):
        return False
    memory = {}
    for line in prefix.splitlines():
        match = _RANGE.fullmatch(line)
        if match:
            memory.setdefault(match[1], []).append((int(match[2], 16), int(match[3], 16)))
    image, stack, handoff, source = (memory[k][0] for k in
                                    (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE'))

    def inside(inner, outer):
        return outer[0] <= inner[0] < inner[1] <= outer[1]

    def overlap(a, b):
        return a[0] < b[1] and b[0] < a[1]

    def reserved(bounds):
        return any(inside(bounds, extent) for extent in memory[b'RESERVED'])

    if (site['image_size'] != image[1] - image[0] or
            (arch == 'x86_64' and (site.get('address_space') != 'image-rva' or
                                  site['image_start'] != 0 or
                                  site['image_end'] != site['image_size'])) or
            (arch == 'aarch64' and
             (site.get('address_space') != 'identity-image-address' or
              image != (site['image_start'], site['image_end'])))):
        return False
    if arch not in ('x86_64', 'aarch64'):
        return False
    relocation = image[0] if arch == 'x86_64' else 0
    objects = site.get('objects')
    pool = site.get('boot_pool')
    if (not isinstance(objects, dict) or set(objects) != {'tables', 'control', 'data'} or
            not isinstance(pool, dict) or set(pool) != {'pool', 'pool_metadata'}):
        return False
    expected_objects, pool_bounds = {}, []
    for name, size in [('tables', 131072), ('control', 4096), ('data', 4096),
                       ('pool', 32768), ('pool_metadata', 4096)]:
        item = objects.get(name) if name in objects else pool.get(name)
        if (not isinstance(item, dict) or any(type(item.get(key)) is not int
                for key in ('start', 'end', 'bytes')) or item['bytes'] != size or
                item['start'] < 0 or item['end'] - item['start'] != size):
            return False
        bounds = item['start'] + relocation, item['end'] + relocation
        if ((bounds[0] | bounds[1]) & 4095 or bounds[1] >= 1 << 64 or
                not inside(bounds, image)):
            return False
        if name in objects:
            expected_objects[name] = bounds
        else:
            pool_bounds.append(bounds)
    tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    if len(tail) < 10:
        return False
    declared = []
    for name, line in zip((b'TABLES', b'CONTROL', b'DATA', b'WORKSPACE'), tail[:4]):
        match = _PAGING_RANGE.fullmatch(line)
        if not match or match[1] != name:
            return False
        bounds = int(match[2], 16), int(match[3], 16)
        if not inside(bounds, image) or not reserved(bounds):
            return False
        declared.append(bounds)
    tables, control, data, workspace = declared
    if ((tables, control, data) != tuple(expected_objects[k] for k in
                                        ('tables', 'control', 'data')) or
            any(overlap(bounds, excluded) for bounds in (tables, control, data)
                for excluded in (stack, handoff, source, workspace, *pool_bounds)) or
            any(overlap(a, b) for i, a in enumerate((tables, control, data))
                for b in (tables, control, data)[i + 1:])):
        return False
    cursor = 4
    if arch == 'x86_64':
        before = _paging_fields(tail[cursor], b'STYLEOS PAGING X86 BEFORE',
                                ('cr0', 'cr3', 'cr4', 'efer', 'pat', 'rflags', 'bits'))
        gdt = _paging_fields(tail[cursor + 1], b'STYLEOS PAGING X86 GDT',
                             ('va', 'pa', 'bytes', 'cs', 'descriptor'))
        idt = _paging_fields(tail[cursor + 2], b'STYLEOS PAGING X86 IDT', ('va', 'bytes'))
        cursor += 3
        if (not before or not gdt or not idt or not 32 <= before['bits'] <= 52 or
                before['cr0'] & 0x80000001 != 0x80000001 or
                before['cr0'] & ((1 << 29) | (1 << 30)) or
                before['cr4'] & ((1 << 12) | (1 << 17) | (1 << 21) |
                                  (1 << 22) | (1 << 23) | (1 << 24)) or
                not before['cr4'] & (1 << 5) or before['efer'] & 0xd00 != 0xd00 or
                before['pat'] & 255 != 6 or not before['cr3'] or
                before['cr3'] & 4095 or before['cr3'] >= 1 << before['bits'] or
                before['rflags'] & (1 << 9) or
                not 0 < gdt['bytes'] <= 4096 or gdt['va'] & 7 or
                not 0 < gdt['cs'] <= 0xfff8 or
                gdt['cs'] & 7 or gdt['cs'] + 8 > gdt['bytes'] or
                (gdt['descriptor'] >> 40 & 0xf8) != 0x98 or
                not gdt['descriptor'] & (1 << 53) or gdt['descriptor'] & (1 << 54) or
                gdt['va'] + gdt['bytes'] > 1 << 47 or
                gdt['va'] & 4095 != gdt['pa'] & 4095 or
                idt['bytes'] != 4096 or
                not inside((idt['va'], idt['va'] + idt['bytes']), image)):
            return False
        ready = f'STYLEOS x86_64 IDT ready cs=0x{gdt["cs"]:016x}'.encode()
        if prefix.splitlines().count(ready) != 1:
            return False
        pa_bits = before['bits']
    else:
        before = _paging_fields(tail[cursor], b'STYLEOS PAGING ARM BEFORE',
                                ('mmfr0', 'sctlr', 'vbar', 'daif'))
        cursor += 1
        if (not before or before['mmfr0'] & 15 > 5 or
                before['mmfr0'] >> 28 & 15 or
                before['sctlr'] & ((1 << 25) | (1 << 24) | (1 << 12) | 5) or
                before['daif'] & 0x3c0 != 0x3c0 or before['vbar'] & 2047 or
                not inside((before['vbar'], before['vbar'] + 2048), image)):
            return False
        pa_bits = (32, 36, 40, 42, 44, 48)[before['mmfr0'] & 15]

    maps = []
    while cursor < len(tail) and tail[cursor].startswith(b'STYLEOS PAGING MAP '):
        match = _PAGING_MAP.fullmatch(tail[cursor])
        if not match or match[1] != b'MAP' or match[4] is None:
            return False
        va, pa, size, flags = (int(value, 16) for value in match.groups()[1:])
        if (not size or (va | pa | size) & 4095 or va + size > 1 << 47 or
                pa + size > 1 << pa_bits or flags & ~7 or
                va <= _PAGING_PROBE < va + size or len(maps) >= 16):
            return False
        if any(overlap((va, va + size), (v, v + n)) for v, _p, n, _f in maps):
            return False
        for _old_va, old_pa, old_size, old_flags in maps:
            if (overlap((pa, pa + size), (old_pa, old_pa + old_size)) and
                    ((flags & 5) != (old_flags & 5) or
                     flags & 2 and not old_flags & 2)):
                return False
        maps.append((va, pa, size, flags))
        cursor += 1
    rounded_image = image[0] & ~4095, (image[1] + 4095) & ~4095
    expected = [(rounded_image[0], rounded_image[0],
                 rounded_image[1] - rounded_image[0], 3),
                (_PAGING_ALIAS, data[0], 4096, 1)]
    if maps[:2] != expected:
        return False
    if arch == 'aarch64':
        if maps[2:] != [(0x9000000, 0x9000000, 4096, 5)]:
            return False
    else:
        gdt_pages = list(range(gdt['va'] & ~4095, (gdt['va'] + gdt['bytes'] + 4095) & ~4095, 4096))
        if any(rounded_image[0] <= va < rounded_image[1] for va in gdt_pages):
            return False
        external = [va for va in gdt_pages if not rounded_image[0] <= va < rounded_image[1]]
        if len(maps[2:]) != len(external):
            return False
        accessed = bool(gdt['descriptor'] & (1 << 40))
        access_page = (gdt['va'] + gdt['cs'] + 5) & ~4095
        for va, (mapped_va, pa, size, flags) in zip(external, maps[2:]):
            expected_flags = int(not accessed and va == access_page)
            if (mapped_va != va or size != 4096 or flags != expected_flags or
                    not reserved((pa, pa + 4096)) or
                    overlap((pa, pa + 4096), image) or
                    (flags & 1 and overlap((pa, pa + 4096), source))):
                return False
        first_page = gdt_pages[0]
        mapping = next((m for m in maps if m[0] <= first_page < m[0] + m[2]), None)
        if not mapping or mapping[1] + gdt['va'] - mapping[0] != gdt['pa']:
            return False
    if cursor >= len(tail):
        return False
    pages = sum(size // 4096 for _va, _pa, size, _flags in maps)
    if pages > 32 * 512:
        return False
    root = _paging_fields(tail[cursor], b'STYLEOS PAGING ROOT', ('pa', 'pages'))
    cursor += 1
    prefixes = [set(), set(), set()]
    for va, _pa, size, _flags in maps:
        for shift, values in zip((39, 30, 21), prefixes):
            values.update(range(va >> shift, ((va + size - 1) >> shift) + 1))
    needed_tables = 1 + sum(map(len, prefixes))
    if (not root or root['pa'] != tables[0] or not 1 <= root['pages'] <= 32 or
            root['pages'] != needed_tables or
            root['pa'] + root['pages'] * 4096 > tables[1] or
            root['pa'] >= 1 << pa_bits or cursor >= len(tail)):
        return False
    if arch == 'x86_64':
        after = _paging_fields(tail[cursor], b'STYLEOS PAGING X86 AFTER',
                               ('cr0', 'cr3', 'cr4', 'efer'))
        if (not after or after['cr3'] != root['pa'] or before['cr3'] == after['cr3'] or
                after['cr0'] != before['cr0'] | (1 << 16) or
                any(after[key] != before[key] for key in ('cr4', 'efer'))):
            return False
    else:
        after = _paging_fields(tail[cursor], b'STYLEOS PAGING ARM AFTER',
                               ('ttbr0', 'ttbr1', 'tcr', 'mair', 'sctlr', 'vbar'))
        tcr = 16 | (3 << 12) | (16 << 16) | (1 << 23) | (2 << 30) | ((before['mmfr0'] & 15) << 32)
        if (not after or after['ttbr0'] != root['pa'] or after['ttbr1'] != 0 or
                after['tcr'] != tcr or after['mair'] != 0x44 or
                after['sctlr'] != 0x30d00801 or after['vbar'] != before['vbar']):
            return False
    cursor += 1
    if cursor + pages >= len(tail):
        return False
    for va, pa, size, flags in maps:
        for offset in range(0, size, 4096):
            expected_pte = (f'STYLEOS PAGING PTE va=0x{va + offset:016x} '
                            f'pa=0x{pa + offset:016x} flags=0x{flags:016x}').encode()
            if tail[cursor] != expected_pte:
                return False
            cursor += 1
    alias = _paging_fields(tail[cursor], b'STYLEOS PAGING ALIAS',
                           ('va', 'pa', 'before', 'after'))
    cursor += 1
    if alias != dict(va=_PAGING_ALIAS, pa=data[0], before=0x123456789abcdef0,
                     after=0xfedcba9876543210):
        return False
    if not fault:
        return ('fault_site' not in site and
                tail[cursor:] == [b'STYLEOS PAGING PASS', b'STYLEOS KERNEL HALT'])
    fault_site = site.get('fault_site')
    if (not isinstance(fault_site, dict) or fault_site.get('symbol') != 'styleos_paging_fault' or
            len(tail[cursor:]) != 3):
        return False
    key, opcode = ('rva', '488b01') if arch == 'x86_64' else ('pc', '010040f9')
    if (type(fault_site.get(key)) is not int or fault_site[key] < 0 or
            fault_site.get('instruction_hex') != opcode):
        return False
    pc = fault_site[key] + relocation
    if not inside((pc, pc + len(bytes.fromhex(opcode))), image):
        return False
    ready = _paging_fields(tail[cursor], b'STYLEOS PAGING FAULT READY', ('va', 'pc'))
    if ready != dict(va=_PAGING_PROBE, pc=pc) or tail[-1] != b'STYLEOS EMERGENCY HALT':
        return False
    record = tail[cursor + 1]
    if arch == 'x86_64':
        match = re.fullmatch(rb'STYLEOS EXCEPTION arch=x86_64 kind=sync vector=14 '
                             rb'reason=page-fault pc=' + _HEX16 + rb' error=' + _HEX16 +
                             rb' cr2=' + _HEX16, record)
        return bool(match and tuple(int(value, 16) for value in match.groups()) ==
                    (pc, 0, _PAGING_PROBE))
    match = re.fullmatch(rb'STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x25 '
                         rb'reason=translation-fault pc=' + _HEX16 + rb' esr=' + _HEX16 +
                         rb' elr=' + _HEX16 + rb' spsr=' + _HEX16 + rb' far=' + _HEX16,
                         record)
    if not match:
        return False
    saved_pc, esr, elr, spsr, far = (int(value, 16) for value in match.groups())
    return (saved_pc == elr == pc and far == _PAGING_PROBE and
            (esr >> 26) & 63 == 0x25 and esr & (1 << 25) and
            4 <= esr & 63 <= 7 and not esr & 0x7c0 and
            spsr & 0x3df == 0x3c5)


def verified_paging_site(folder, arch, fault=False):
    """Rederive the full image/fault proof and bind every actual loaded input."""
    target = folder / arch
    site = json.loads((target / 'paging-site.json').read_text())
    if arch == 'x86_64':
        verified = describe_paging(arch, target / 'BOOTX64.EFI', target / 'kernel.map', fault=fault)
        verified['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
    else:
        verified = describe_paging(arch, target / 'kernel.elf', raw_path=target / 'styleos.bin', fault=fault)
    if site != verified:
        raise ValueError('paging-site metadata differs from independently checked image/fault proof/SHA-256')
    manifest = json.loads((folder / 'manifest.json').read_text())
    expected_modes = {'invalid_boot_info': False, 'invalid_memory_map': False,
                      'test_exception': False, 'test_bootpool': False,
                      'test_paging': not fault, 'test_paging_fault': fault}
    if (not isinstance(manifest, dict) or
            any(type(manifest.get(key)) is not bool or manifest[key] != value
                for key, value in expected_modes.items())):
        raise ValueError('paging manifest mode mismatch')
    artifacts = manifest.get('artifacts')
    if (not isinstance(artifacts, list) or any(not isinstance(row, dict) or
            not isinstance(row.get('path'), str) for row in artifacts)):
        raise ValueError('paging manifest artifacts are invalid')
    names = (('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')) if arch == 'x86_64' else (
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256'))
    selected = [row for row in artifacts if row['path'].startswith(arch + '/')]
    if len(selected) != 2:
        raise ValueError('paging manifest needs exactly two artifacts for selected architecture')
    for name, key in names:
        matches = [row for row in selected if row['path'] == arch + '/' + name]
        if (len(matches) != 1 or type(matches[0].get('size')) is not int or
                matches[0]['size'] != (target / name).stat().st_size or
                matches[0].get('sha256') != verified.get(key)):
            raise ValueError('paging manifest artifact size/SHA-256 mismatch')
    return verified


def serial_idt_ready(output):
    """An actual-CS IDT-ready marker must follow EBS and precede common C."""
    lines = output.splitlines()
    ready = [(i, re.fullmatch(rb'STYLEOS x86_64 IDT ready cs=0x([0-9a-f]{16})', line))
             for i, line in enumerate(lines) if line.startswith(b'STYLEOS x86_64 IDT ready')]
    try:
        if len(ready) != 1 or not ready[0][1]:
            return False
        index, match = ready[0]
        selector = int(match[1], 16)
        return (0 < selector <= 0xfff8 and selector & 7 == 0 and
                lines.index(b'STYLEOS x86_64 ExitBootServices OK') < index <
                lines.index(b'STYLEOS kernel_main entered'))
    except ValueError:
        return False


class Checks:
    def __init__(self, output):
        self.output = output
        self.logs = output / 'test-results'
        self.logs.mkdir(parents=True, exist_ok=True)
        self.results = []

    def record(self, name, status, detail, text=''):
        log = self.logs / (name + '.log')
        log.write_text(text, encoding='utf-8')
        self.results.append({'name': name, 'status': status, 'detail': detail,
                             'log': log.name})
        print(f'{name}: {status} — {detail}', flush=True)

    def command(self, name, commands):
        logs = []
        for cmd in commands:
            cmd = list(map(str, cmd))
            logs.append('$ ' + ' '.join(cmd))
            try:
                run = subprocess.run(cmd, cwd=ROOT, text=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     timeout=180)
            except (OSError, subprocess.TimeoutExpired) as error:
                self.record(name, 'FAIL', str(error), '\n'.join(logs))
                return False
            logs.append(run.stdout)
            if run.returncode:
                self.record(name, 'FAIL', f'exit {run.returncode}', '\n'.join(logs))
                return False
        self.record(name, 'PASS', 'exit 0', '\n'.join(logs))
        return True

    def host(self):
        cc = os.environ.get('CC', 'cc')
        folder = self.output / 'host-tests'
        folder.mkdir(parents=True, exist_ok=True)
        flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-Iinclude', '-Iboot/x86_64']
        for name, sources in [
            ('host-boot-info', ['tests/test_boot_info.c', 'kernel/boot_info.c']),
            ('host-uefi', ['tests/test_uefi.c', 'boot/x86_64/exit_boot.c']),
            ('host-exception', ['tests/test_exception.c', 'kernel/exception.c']),
            ('host-pool', ['tests/test_boot_pool.c', 'kernel/boot_pool.c']),
            ('host-paging', ['tests/test_paging.c', 'kernel/paging.c']),
            ('host-memory', ['tests/test_boot_memory.c', 'kernel/boot_memory.c',
                             'boot/x86_64/memory_map.c', 'boot/aarch64/fdt_memory.c'])
        ]:
            exe = folder / name
            commands = [[cc, *flags, *sources, '-o', exe], [exe]]
            if name == 'host-paging':
                admission = folder / 'test-paging-admission'
                commands += [[cc, *flags, 'tests/test_paging_admission.c',
                              'kernel/paging_admission.c', 'kernel/boot_pool.c',
                              '-o', admission], [admission]]
                x86 = folder / 'test-paging-x86'
                commands += [[cc, *flags, 'tests/test_paging_x86.c', '-o', x86], [x86]]
            self.command(name, commands)
        self.command('host-python', [[sys.executable, '-m', 'unittest', 'discover',
                                    '-s', 'tests', '-p', 'test_*.py', '-v']])

    def build(self, folder, mode='normal', name='build', arch='all'):
        cmd = [sys.executable, ROOT / 'tools/build.py', '--arch', arch,
               '--output', folder]
        if mode != 'normal':
            cmd.append({'invalid': '--invalid-boot-info',
                        'invalid-map': '--invalid-memory-map',
                        'exception': '--test-exception',
                        'bootpool': '--test-bootpool',
                        'paging': '--test-paging',
                        'paging-fault': '--test-paging-fault'}[mode])
        return self.command(name, [cmd])

    def repro(self):
        one, two = self.output / 'repro-one', self.output / 'repro-two'
        if not self.build(one, name='repro-build-one'):
            return
        if not self.build(two, name='repro-build-two'):
            return
        a = json.loads((one / 'manifest.json').read_text())['artifacts']
        b = json.loads((two / 'manifest.json').read_text())['artifacts']
        if a != b:
            self.record('reproducibility', 'FAIL', 'artifact manifests differ',
                        json.dumps({'one': a, 'two': b}, indent=2))
            return
        for row in a:
            for directory in (one, two):
                blob = (directory / row['path']).read_bytes()
                if hashlib.sha256(blob).hexdigest() != row['sha256']:
                    self.record('reproducibility', 'FAIL', 'artifact hash mismatch')
                    return
        self.record('reproducibility', 'PASS', 'all 4 artifacts identical',
                    json.dumps(a, indent=2))

    def qemu(self, timeout, arches):
        build_arch = arches[0] if len(arches) == 1 else 'all'
        folders = {'normal': self.output, 'invalid': self.output / 'negative',
                   'invalid-map': self.output / 'negative-memory',
                   'exception': self.output / 'exception',
                   'bootpool': self.output / 'bootpool',
                   'paging': self.output / 'paging',
                   'paging-fault': self.output / 'paging-fault'}
        names = ('build-for-qemu', 'build-negative', 'build-negative-memory',
                 'build-exception', 'build-bootpool', 'build-paging',
                 'build-paging-fault')
        for (mode, folder), name in zip(folders.items(), names):
            if not self.build(folder, mode, name, build_arch):
                return
        jobs = []
        for arch, variable, default in [
            ('x86_64', 'QEMU_X86', 'qemu-system-x86_64'),
            ('aarch64', 'QEMU_AARCH64', 'qemu-system-aarch64')
        ]:
            if arch not in arches:
                continue
            binary = shutil.which(os.environ.get(variable, default))
            if not binary:
                self.record('qemu-' + arch, 'SKIPPED', default + ' not found')
                continue
            firmware = None
            if arch == 'x86_64':
                code = firmware_path('OVMF_CODE', 'OVMF_CODE')
                var = firmware_path('OVMF_VARS', 'OVMF_VARS')
                if not code or not var:
                    self.record('qemu-x86_64', 'SKIPPED', 'OVMF_CODE/OVMF_VARS missing')
                    continue
                firmware = (code, var)
            for mode, folder in folders.items():
                jobs.append((f'qemu-{arch}-{mode}', arch, binary, folder,
                             firmware, mode, False, 128, timeout))
            if arch == 'aarch64':
                jobs.append(('qemu-aarch64-exception-el2', arch, binary, folders['exception'],
                             None, 'exception', True, 128, timeout))
                jobs.append(('qemu-aarch64-bootpool-el2', arch, binary, folders['bootpool'],
                             None, 'bootpool', True, 128, timeout))
                for mode in ('paging', 'paging-fault'):
                    jobs.append((f'qemu-aarch64-{mode}-el2', arch, binary, folders[mode],
                                 None, mode, True, 128, timeout))
                jobs.extend((f'qemu-{arch}-{suffix}', arch, binary, self.output,
                             None, 'normal', el2, mib, timeout)
                            for suffix, el2, mib in [('el2', True, 128), ('256m', False, 256)])
        with ThreadPoolExecutor(max_workers=3) as pool:
            for name, status, detail, text in pool.map(self.run_qemu, jobs):
                self.record(name, status, detail, text)

    def run_qemu(self, job):
        name, arch, binary, folder, firmware, mode, el2, mib, timeout = job
        scratch = self.logs / name
        scratch.mkdir(exist_ok=True)
        cmd = [binary, '-accel', 'tcg,thread=single', '-m', f'{mib}M', '-display', 'none',
               '-serial', 'stdio', '-monitor', 'none', '-net', 'none', '-no-reboot']
        if arch == 'x86_64':
            code, var = firmware
            vars_copy = scratch / 'OVMF_VARS.fd'
            shutil.copyfile(var, vars_copy)
            cmd += ['-machine', 'q35', '-vga', 'none', '-drive', f'if=pflash,format=raw,readonly=on,file={code}',
                    '-drive', f'if=pflash,format=raw,file={vars_copy}',
                    '-drive', f'if=none,id=styleos,format=raw,readonly=on,file={folder / arch / "styleos.img"}',
                    '-device', 'virtio-blk-pci,drive=styleos']
        else:
            machine = 'virt,virtualization=on' if el2 else 'virt'
            cmd += ['-machine', machine, '-cpu', 'cortex-a57', '-kernel',
                    str(folder / arch / 'styleos.bin')]
        expected = {'invalid': b'STYLEOS BOOT_INFO ERROR: magic',
                    'invalid-map': b'STYLEOS MEMMAP ERROR: ',
                    'exception': b'STYLEOS EMERGENCY HALT',
                    'bootpool': b'STYLEOS KERNEL HALT',
                    'paging': b'STYLEOS KERNEL HALT',
                    'paging-fault': b'STYLEOS EMERGENCY HALT',
                    'normal': b'STYLEOS KERNEL HALT'}[mode]
        output = bytearray()
        started = time.monotonic()
        process = None
        terminal_seen_at = None
        settled = False
        natural_exit = False
        requested_stop = False
        forced_stop = False
        selector = selectors.DefaultSelector()
        try:
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            selector.register(process.stdout, selectors.EVENT_READ)
            while time.monotonic() - started < timeout and len(output) < 4 * 1024 * 1024:
                for key, _ in selector.select(timeout=0.1):
                    chunk = os.read(key.fileobj.fileno(), 65536)
                    if chunk:
                        output.extend(chunk)
                    else:
                        selector.unregister(key.fileobj)
                if (terminal_seen_at is None and
                        any(line.startswith(expected) for line in output.split(b'\n')[:-1])):
                    terminal_seen_at = time.monotonic()
                if process.poll() is not None:
                    natural_exit = True
                    break
                # A marker followed immediately by reset/exit is not a halt.
                # Keep collecting output briefly before our intentional stop.
                if terminal_seen_at is not None and time.monotonic() - terminal_seen_at >= 0.2:
                    settled = True
                    break
        except OSError as error:
            return name, 'FAIL', str(error), '$ ' + ' '.join(cmd)
        finally:
            selector.close()
            if process is not None:
                if process.poll() is None:
                    requested_stop = True
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        forced_stop = True
                        process.kill()
                        process.wait()
                else:
                    natural_exit = True
                if process.stdout:
                    output.extend(process.stdout.read())
                    process.stdout.close()
        text = '$ ' + ' '.join(cmd) + '\n' + output.decode('utf-8', 'replace')
        text += (f'\n[harness] requested_stop={requested_stop} forced_stop={forced_stop} '
                 f'natural_exit={natural_exit} returncode={process.returncode} settled={settled}\n')
        if expected not in output:
            return name, 'FAIL', 'expected serial marker missing / timeout', text
        if natural_exit or not requested_stop:
            return name, 'FAIL', 'QEMU exited/reset before intentional harness stop', text
        # QEMU may handle SIGTERM and return 0; an unrelated error/abort is not OK.
        if process.returncode not in ((0, -9) if forced_stop else (0, -15)):
            return name, 'FAIL', 'unexpected QEMU process exit status', text
        if not settled:
            return name, 'FAIL', 'terminal marker observation window incomplete', text
        if mode == 'exception':
            try:
                site = json.loads((folder / arch / 'fault-site.json').read_text())
                executable = folder / arch / ('BOOTX64.EFI' if arch == 'x86_64' else 'kernel.elf')
                loaded = folder / arch / ('styleos.img' if arch == 'x86_64' else 'styleos.bin')
                loaded_key = 'disk_sha256' if arch == 'x86_64' else 'raw_image_sha256'
                if (hashlib.sha256(executable.read_bytes()).hexdigest() != site.get('binary_sha256') or
                        hashlib.sha256(loaded.read_bytes()).hexdigest() != site.get(loaded_key)):
                    return name, 'FAIL', 'fault-site metadata/image SHA-256 mismatch', text
            except (OSError, ValueError) as error:
                return name, 'FAIL', 'fault-site metadata unavailable: ' + str(error), text
            good = serial_exception_ok(output, arch, site, mib, el2)
        elif mode == 'bootpool':
            try:
                site = verified_pool_site(folder, arch)
            except (OSError, ValueError) as error:
                return name, 'FAIL', 'pool-site proof unavailable/mismatched: ' + str(error), text
            good = serial_pool_ok(output, arch, site, mib, el2)
        elif mode in ('paging', 'paging-fault'):
            try:
                site = verified_paging_site(folder, arch, mode == 'paging-fault')
            except (OSError, ValueError) as error:
                return name, 'FAIL', 'paging-site proof unavailable/mismatched: ' + str(error), text
            good = serial_paging_ok(output, arch, site, mode == 'paging-fault', mib, el2)
        else:
            good = serial_memory_ok(output, arch, mode, mib, el2)
        if arch == 'x86_64':
            good = good and serial_idt_ready(output)
        if mode in ('paging', 'paging-fault'):
            (self.logs / (name + '.json')).write_text(json.dumps({
                'arch': arch, 'mode': mode, 'initial_el2': el2,
                'ram_mib': mib, 'status': 'PASS' if good else 'FAIL',
                'uart_sha256': hashlib.sha256(output).hexdigest(),
                'manifest_sha256': hashlib.sha256((folder / 'manifest.json').read_bytes()).hexdigest(),
                'paging_site': site, 'requested_stop': requested_stop,
                'forced_stop': forced_stop, 'natural_exit': natural_exit,
                'returncode': process.returncode, 'settled': settled
            }, indent=2) + '\n')
        return name, ('PASS' if good else 'FAIL'), 'UART handoff/memory/exception/pool/paging contract checked; QEMU stopped by harness', text


def firmware_path(variable, prefix):
    configured = os.environ.get(variable)
    if configured:
        path = Path(configured)
        return path if path.is_file() else None
    for root in ('/usr/share/OVMF', '/usr/share/edk2/x64'):
        for suffix in ('_4M.fd', '.fd'):
            path = Path(root) / (prefix + suffix)
            if path.is_file():
                return path
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('host', 'repro', 'qemu', 'all'):
        parser.add_argument('--' + flag, action='store_true')
    parser.add_argument('--qemu-arch', choices=['all', 'aarch64', 'x86_64'], default='all',
                        help='Select only the QEMU checks and their builds; use with --qemu')
    parser.add_argument('--output', type=Path, default=ROOT / 'build')
    parser.add_argument('--timeout', type=float, default=25)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    if args.qemu_arch != 'all' and (not args.qemu or args.all):
        parser.error('--qemu-arch requires --qemu without --all')
    checks = Checks(args.output.resolve())
    all_checks = args.all or not (args.host or args.repro or args.qemu)
    if args.host or all_checks:
        checks.host()
    if args.repro or all_checks:
        checks.repro()
    if args.qemu or all_checks:
        arches = ('x86_64', 'aarch64') if args.qemu_arch == 'all' else (args.qemu_arch,)
        checks.qemu(args.timeout, arches)
    (checks.logs / 'results.json').write_text(json.dumps(checks.results, indent=2) + '\n')
    failed = any(r['status'] == 'FAIL' for r in checks.results)
    skipped = any(r['status'] == 'SKIPPED' for r in checks.results)
    # Missing required emulators is visible to CI, not a successful green run.
    return 1 if failed else (2 if skipped else 0)


if __name__ == '__main__':
    sys.exit(main())
