#!/usr/bin/env python3
"""Host, reproducible-build and bounded real-QEMU checks for T-P1.7."""
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

try:
    from .ownership_site import describe_ownership
except ImportError:  # Direct tools/test.py invocation.
    from ownership_site import describe_ownership

try:
    from .tasks_site import describe_tasks
except ImportError:  # Direct tools/test.py invocation.
    from tasks_site import describe_tasks

ROOT = Path(__file__).resolve().parents[1]


def serial_handoff_ok(output, arch, invalid=False, el2=False):
    """A successful marker cannot erase a prior boot failure in the same run."""
    errors = (b'BOOT ERROR', b'BOOT_ERROR', b'early exception',
              b'STYLEOS EXCEPTION', b'STYLEOS EMERGENCY', b'STYLEOS BOOTPOOL',
              b'STYLEOS PAGING', b'STYLEOS OWNERSHIP', b'STYLEOS TASKS',
              b'STYLEOS USER')
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


_OWNERSHIP_CLAIM = re.compile(
    rb'STYLEOS OWNERSHIP CLAIM kind=([A-Z_]+) proof=([A-Z_]+) va=' + _HEX16 +
    rb' pa=' + _HEX16 + rb' bytes=' + _HEX16)
_OWNERSHIP_RANGE = re.compile(
    rb'STYLEOS OWNERSHIP (PROTECTED|REVIEW) ' + _HEX16 + rb' ' + _HEX16 +
    rb'( reason=no-transfer)?')
_OWNERSHIP_STATE_FIELDS = ('mode', 'root', 'aux0', 'aux1', 'pat', 'rflags', 'bits',
                           'features', 'gdt_va', 'gdt_bytes', 'idt_va', 'idt_bytes', 'cs')


def _ownership_union(ranges):
    result = []
    for a, b in sorted(ranges):
        if result and a <= result[-1][1]:
            result[-1] = result[-1][0], max(b, result[-1][1])
        else:
            result.append((a, b))
    return result


def _ownership_subtract(candidates, protected):
    result = []
    for a, b in candidates:
        for c, d in protected:
            if d <= a:
                continue
            if c >= b:
                break
            if a < c:
                result.append((a, c))
            a = max(a, d)
            if a >= b:
                break
        if a < b:
            result.append((a, b))
    return _ownership_union(result)


def serial_ownership_ok(output, arch, site, invalid=False, ram_mib=128, el2=False):
    """Reconstruct the whole declared read-only plan from binary and UART facts.

    Claims are dependencies, never grants. Inherited x86 table coverage remains
    explicitly incomplete; only pages observed by the bounded collector can be
    checked, and no claim turns firmware reservations into reviewable RAM.
    """
    try:
        return _serial_ownership_ok(output, arch, site, invalid, ram_mib, el2)
    except (KeyError, TypeError, ValueError, IndexError, OverflowError):
        return False


def _serial_ownership_ok(output, arch, site, invalid, ram_mib, el2):
    if (not isinstance(site, dict) or site.get('arch') != arch or
            arch not in ('x86_64', 'aarch64') or type(invalid) is not bool or
            site.get('invalid') is not invalid or site.get('page_size') != 4096 or
            type(site.get('page_size')) is not int or
            b'timeout' in output.lower() or b'timed out' in output.lower()):
        return False
    lines = output.splitlines()
    first = next((i for i, line in enumerate(lines)
                  if line.startswith(b'STYLEOS OWNERSHIP')), None)
    if first is None:
        return False
    prefix = b'\n'.join(lines[:first]) + b'\n'
    if not serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False):
        return False
    tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    if invalid:
        # Required-claim refusal publishes no partial claims, range or PASS.
        return tail == [b'STYLEOS OWNERSHIP ERROR: required']
    if len(tail) < 15:
        return False
    memory = {}
    for line in prefix.splitlines():
        match = _RANGE.fullmatch(line)
        if match:
            memory.setdefault(match[1], []).append((int(match[2], 16), int(match[3], 16)))
    if any(len(memory.get(key, [])) > 256 for key in (b'RAM', b'RESERVED', b'CANDIDATE')):
        return False
    image, stack, handoff, source = (memory[k][0] for k in
                                    (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE'))
    phase, provenance = ('inherited', 'uefi') if arch == 'x86_64' else ('mmu-off', 'fdt')
    if tail[0] != f'STYLEOS OWNERSHIP PLAN arch={arch} phase={phase} source={provenance}'.encode():
        return False
    image_keys = ('image_start', 'image_end', 'image_size')
    if (any(type(site.get(k)) is not int or not 0 <= site[k] < 1 << 64 for k in image_keys) or
            site['image_size'] != image[1] - image[0]):
        return False
    if arch == 'x86_64':
        if (site.get('address_space') != 'image-rva' or site['image_start'] != 0 or
                site['image_end'] != site['image_size']):
            return False
        relocation = image[0]
    else:
        if (site.get('address_space') != 'identity-image-address' or
                image != (site['image_start'], site['image_end']) or
                any(type(site.get(k)) is not int for k in ('bss_start', 'bss_end')) or
                not image[0] <= site['bss_start'] < site['bss_end'] <= image[1]):
            return False
        relocation = 0
    sizes = {'workspace': 65536, 'diagnostic': 12400, 'stack': 65536,
             'pool': 32768, 'pool_metadata': 4096}
    sizes.update({'idt': 4096, 'handoff': 96} if arch == 'x86_64' else {'vectors': 2048})
    objects = site.get('objects')
    if not isinstance(objects, dict) or set(objects) != set(sizes):
        return False
    expected = {}
    for name, size in sizes.items():
        item = objects[name]
        if (not isinstance(item, dict) or
                any(type(item.get(k)) is not int or not 0 <= item[k] < 1 << 64
                    for k in ('start', 'end', 'bytes')) or item['bytes'] != size or
                item['end'] - item['start'] != size):
            return False
        a, b = item['start'] + relocation, item['end'] + relocation
        if not image[0] <= a < b <= image[1]:
            return False
        alignment = {'stack': 16, 'diagnostic': 8, 'handoff': 8, 'vectors': 2048,
                     'idt': 16}.get(name, 4096)
        if a % alignment:
            return False
        expected[name] = a, b
    if (expected['stack'] != stack or
            (arch == 'x86_64' and expected['handoff'] != handoff) or
            any(a < d and c < b for i, (a, b) in enumerate(expected.values())
                for c, d in list(expected.values())[i + 1:])):
        return False
    own = _paging_fields(tail[1], b'STYLEOS OWNERSHIP WORKSPACE', ('pa', 'bytes'))
    origin = _paging_fields(tail[2], b'STYLEOS OWNERSHIP SOURCE', ('pa', 'bytes', 'stride', 'version'))
    state = _paging_fields(tail[3], b'STYLEOS OWNERSHIP STATE', _OWNERSHIP_STATE_FIELDS)
    if (not own or not origin or not state or
            (own['pa'], own['pa'] + own['bytes']) != expected['workspace'] or
            (origin['pa'], origin['pa'] + origin['bytes']) != source or
            not 32 <= state['bits'] <= (52 if arch == 'x86_64' else 48) or
            (arch == 'aarch64' and state['bits'] not in (32, 36, 40, 42, 44, 48))):
        return False
    limit = 1 << state['bits']
    if any(b > limit for ranges in memory.values() for _a, b in ranges):
        return False
    if arch == 'x86_64':
        if (origin['version'] != 1 or origin['stride'] < 40 or origin['stride'] & 7 or
                not origin['bytes'] or origin['bytes'] % origin['stride'] or
                state['mode'] & 0x80000001 != 0x80000001 or state['mode'] & 0x60000000 or
                not state['aux0'] & (1 << 5) or
                state['aux0'] & sum(1 << bit for bit in (12, 17, 21, 22, 23, 24)) or
                state['aux1'] & 0xd00 != 0xd00 or state['pat'] & 255 != 6 or
                state['rflags'] & ((1 << 9) | (1 << 10)) or
                state['features'] not in (1, 3) or not state['root'] or
                state['root'] & 4095 or state['root'] + 4096 > limit or
                not 0 < state['gdt_bytes'] <= 65536 or state['gdt_va'] & 7 or
                state['gdt_va'] + state['gdt_bytes'] > 1 << 47 or
                not 0 < state['cs'] <= 0xfff8 or state['cs'] & 7 or
                state['cs'] + 8 > state['gdt_bytes'] or
                (state['idt_va'], state['idt_va'] + state['idt_bytes']) != expected['idt'] or
                prefix.splitlines().count(
                    f'STYLEOS x86_64 IDT ready cs=0x{state["cs"]:016x}'.encode()) != 1):
            return False
    elif (origin['stride'] or origin['version'] or not 40 <= origin['bytes'] <= 16 * 1024 * 1024 or
          state['root'] or state['mode'] & ((1 << 25) | (1 << 24) | (1 << 12) | 5) or
          state['aux0'] != expected['vectors'][0] or state['aux1'] & 0x3c0 != 0x3c0 or
          any(state[key] for key in ('pat', 'rflags', 'features', 'gdt_va', 'gdt_bytes', 'idt_va', 'idt_bytes', 'cs'))):
        return False
    common = {'IMAGE': image, 'STACK': stack, 'HANDOFF': handoff, 'SOURCE_BUFFER': source,
              'MAP_WORKSPACE': expected['diagnostic'], 'OWN_WORKSPACE': expected['workspace'],
              'BOOT_POOL': expected['pool'], 'POOL_METADATA': expected['pool_metadata']}
    claims, cursor = [], 4
    while cursor < len(tail) and tail[cursor].startswith(b'STYLEOS OWNERSHIP CLAIM'):
        match = _OWNERSHIP_CLAIM.fullmatch(tail[cursor])
        if not match or len(claims) >= 96:
            return False
        kind, proof = (part.decode() for part in match.groups()[:2])
        va, pa, size = (int(part, 16) for part in match.groups()[2:])
        if (not size or pa + size >= 1 << 64 or va + size >= 1 << 64 or
                ((pa + size + 4095) & ~4095) > limit):
            return False
        claim = kind, proof, va, pa, size
        if claim in claims:
            return False
        claims.append(claim)
        cursor += 1
    # All common live ranges must appear once, with complete byte extents.
    for kind, (a, b) in common.items():
        found = [c for c in claims if c[0] == kind]
        proof = 'BOOT_SOURCE' if kind == 'SOURCE_BUFFER' else 'IMAGE_IDENTITY'
        if found != [(kind, proof, a, a, b - a)]:
            return False
    extra = [c for c in claims if c[0] not in common]
    def reserved(pa, size):
        return any(a <= pa and pa + size <= b for a, b in memory[b'RESERVED'])
    if arch == 'aarch64':
        a, b = expected['vectors']
        if set(extra) != {('VECTORS', 'IMAGE_IDENTITY', a, a, b - a),
                          ('MMIO', 'FIXED_DEVICE', 0x9000000, 0x9000000, 4096)}:
            return False
    else:
        allowed = {'GDT', 'IDT', 'ROOT', 'OLD_TABLE'}
        if any(c[0] not in allowed for c in extra):
            return False
        idt, root = [c for c in extra if c[0] == 'IDT'], [c for c in extra if c[0] == 'ROOT']
        if (idt != [('IDT', 'OLD_WALK', state['idt_va'], state['idt_va'], 4096)] or
                root != [('ROOT', 'REGISTER', 0, state['root'], 4096)]):
            return False
        gdts = [c for c in extra if c[0] == 'GDT']
        pages = list(range(state['gdt_va'] & ~4095,
                           (state['gdt_va'] + state['gdt_bytes'] + 4095) & ~4095, 4096))
        if (sorted(c[2] for c in gdts) != pages or
                any(proof != 'OLD_WALK' or (va | pa) & 4095 or size != 4096 or
                    not reserved(pa, size) for _k, proof, va, pa, size in gdts)):
            return False
        tables = [c for c in extra if c[0] == 'OLD_TABLE']
        if (not 0 < len(tables) <= 64 or state['root'] not in [c[3] for c in tables] or
                len({c[3] for c in tables}) != len(tables) or
                any(proof != 'OLD_WALK' or va != pa or pa & 4095 or size != 4096 or
                    not reserved(pa, size) for _k, proof, va, pa, size in tables) or
                not reserved(state['root'], 4096)):
            return False
        # Conflicting claims for the same VA cannot prove two translations.
        translated = [c for c in claims if c[0] != 'ROOT']
        for i, (_k, _p, va, pa, size) in enumerate(translated):
            for _ok, _op, ova, opa, osize in translated[:i]:
                start = max(va, ova)
                if start < min(va + size, ova + osize) and pa + start - va != opa + start - ova:
                    return False
    # Independently replay every recorded descriptor path. The same entry
    # cannot have contradictory values in two walks, and every visited table
    # must have its own complete identity proof in this bounded closure.
    walks, entry_values = [], {}
    old_tables = {c[3] for c in claims if c[0] == 'OLD_TABLE'}
    while cursor < len(tail) and tail[cursor].startswith(b'STYLEOS OWNERSHIP WALK'):
        walk = re.fullmatch(rb'STYLEOS OWNERSHIP WALK kind=(GDT|TABLE) va=' + _HEX16 +
                            rb' pa=' + _HEX16 + rb' levels=' + _HEX16, tail[cursor])
        if arch != 'x86_64' or not walk or len(walks) >= 66:
            return False
        kind, va, pa, levels = walk.groups()
        kind = kind.decode()
        va, pa, levels = (int(v, 16) for v in (va, pa, levels))
        if not 2 <= levels <= 4 or (va | pa) & 4095 or va >= 1 << 47:
            return False
        if ((kind == 'GDT' and ('GDT', 'OLD_WALK', va, pa, 4096) not in claims) or
                (kind == 'TABLE' and (va != pa or pa not in old_tables))):
            return False
        if (kind, va, pa) in walks:
            return False
        cursor += 1
        table = state['root']
        address_mask = 0x000ffffffffff000
        pa_mask = (limit - 1) & address_mask
        for level in range(levels):
            step = _paging_fields(tail[cursor], b'STYLEOS OWNERSHIP STEP',
                                   ('level', 'table', 'entry'))
            if not step or step['level'] != level or step['table'] != table or table not in old_tables:
                return False
            entry, shift = step['entry'], 39 - 9 * level
            index = va >> shift & 511
            key = table, index
            if key in entry_values and (entry_values[key] ^ entry) & ~0x60:
                return False
            entry_values[key] = entry
            if (not entry & 1 or entry & address_mask & ~pa_mask or
                    entry & 0x7800000000000000):
                return False
            large = level < 3 and bool(entry & 128)
            if large and (level == 0 or (level == 1 and not state['features'] & 2)):
                return False
            leaf = level == 3 or large
            if leaf != (level == levels - 1):
                return False
            if leaf:
                leaf_shift = shift if large else 12
                offset_mask = (1 << leaf_shift) - 1
                if large and entry & address_mask & offset_mask & ~(1 << 12):
                    return False
                mapped = (entry & pa_mask & ~offset_mask) | (va & offset_mask)
                pat_index = (entry >> 3 & 3) | ((entry >> (12 if large else 7) & 1) << 2)
                if mapped != pa or (state['pat'] >> (8 * pat_index) & 255) != 6:
                    return False
            else:
                if entry & 0x18:
                    return False
                table = entry & pa_mask & address_mask
            cursor += 1
        walks.append((kind, va, pa))
    if arch == 'x86_64':
        expected_walks = {('GDT', c[2], c[3]) for c in claims if c[0] == 'GDT'}
        expected_walks.update(('TABLE', pa, pa) for pa in old_tables)
        if set(walks) != expected_walks:
            return False
    if tail[cursor:cursor + 2] != [b'STYLEOS OWNERSHIP INACTIVE paging=not-built pool=not-admitted',
                                   f'STYLEOS OWNERSHIP UNRESOLVED flags=0x{3 if arch == "x86_64" else 0:016x}'.encode()]:
        return False
    cursor += 2
    protected = _ownership_union(memory[b'RESERVED'] +
                                 [(pa & ~4095, (pa + size + 4095) & ~4095)
                                  for _kind, _proof, _va, pa, size in claims])
    review = _ownership_subtract(memory[b'CANDIDATE'], protected)
    for kind, ranges in ((b'PROTECTED', protected), (b'REVIEW', review)):
        for a, b in ranges:
            match = _OWNERSHIP_RANGE.fullmatch(tail[cursor])
            if (not match or match[1] != kind or (int(match[2], 16), int(match[3], 16)) != (a, b) or
                    bool(match[4]) != (kind == b'REVIEW')):
                return False
            cursor += 1
    if tail[cursor] != b'STYLEOS OWNERSHIP UNCLASSIFIED outside-covered-union':
        return False
    totals = _paging_fields(tail[cursor + 1], b'STYLEOS OWNERSHIP TOTAL',
                             ('protected', 'review', 'granted'))
    after = _paging_fields(tail[cursor + 2], b'STYLEOS OWNERSHIP STATE AFTER',
                           _OWNERSHIP_STATE_FIELDS)
    return (totals == {'protected': sum(b - a for a, b in protected) // 4096,
                       'review': sum(b - a for a, b in review) // 4096, 'granted': 0} and
            after is not None and
            all(after[k] == state[k] for k in state if k != 'rflags') and
            not (after['rflags'] ^ state['rflags']) & ~0x8d5 and
            not after['rflags'] & ((1 << 9) | (1 << 10)) and
            (arch == 'x86_64' or after['rflags'] == 0) and tail[cursor + 3:] == [b'STYLEOS OWNERSHIP PASS', b'STYLEOS KERNEL HALT'])


def ownership_report(output, arch):
    """Serialize evidence only after serial_ownership_ok has accepted it."""
    lines = output.splitlines()
    report = {'arch': arch, 'phase': 'inherited' if arch == 'x86_64' else 'mmu-off',
              'source': 'uefi' if arch == 'x86_64' else 'fdt',
              'protected': [], 'review': [], 'claims': [], 'walks': [],
              'unclassified': 'outside-covered-union', 'granted': 0}
    for line in lines:
        for label, key, fields in (
            (b'WORKSPACE', 'workspace', ('pa', 'bytes')),
            (b'SOURCE', 'source_data', ('pa', 'bytes', 'stride', 'version')),
            (b'STATE', 'state_before', _OWNERSHIP_STATE_FIELDS),
            (b'STATE AFTER', 'state_after', _OWNERSHIP_STATE_FIELDS),
            (b'UNRESOLVED', 'unresolved', ('flags',)),
            (b'TOTAL', 'totals', ('protected', 'review', 'granted'))):
            values = _paging_fields(line, b'STYLEOS OWNERSHIP ' + label, fields)
            if values is not None:
                report[key] = values
        claim = _OWNERSHIP_CLAIM.fullmatch(line)
        if claim:
            kind, proof, va, pa, size = claim.groups()
            report['claims'].append(dict(kind=kind.decode(), proof=proof.decode(),
                                         va=int(va, 16), pa=int(pa, 16), bytes=int(size, 16)))
        bounds = _OWNERSHIP_RANGE.fullmatch(line)
        if bounds:
            kind, start, end, _reason = bounds.groups()
            item = dict(start=int(start, 16), end=int(end, 16))
            if kind == b'REVIEW':
                item['reason'] = 'no-transfer'
            report['protected' if kind == b'PROTECTED' else 'review'].append(item)
        walk = re.fullmatch(rb'STYLEOS OWNERSHIP WALK kind=(GDT|TABLE) va=' + _HEX16 +
                            rb' pa=' + _HEX16 + rb' levels=' + _HEX16, line)
        if walk:
            kind, va, pa, levels = walk.groups()
            report['walks'].append(dict(kind=kind.decode(), va=int(va, 16),
                                        pa=int(pa, 16), levels=int(levels, 16), steps=[]))
        step = _paging_fields(line, b'STYLEOS OWNERSHIP STEP', ('level', 'table', 'entry'))
        if step is not None:
            report['walks'][-1]['steps'].append(step)
    report['inactive'] = {'paging': 'not-built', 'pool': 'not-admitted'}
    report['unresolved']['dependencies'] = (['inherited-table-set-incomplete', 'other-firmware-state']
                                             if arch == 'x86_64' else [])
    return report


def verified_ownership_site(folder, arch, invalid=False):
    """Rebuild the binary proof and bind modes and artifacts actually booted."""
    target = folder / arch
    site = json.loads((target / 'ownership-site.json').read_text())
    if arch == 'x86_64':
        verified = describe_ownership(arch, target / 'BOOTX64.EFI', target / 'kernel.map', invalid=invalid)
        verified['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
    else:
        verified = describe_ownership(arch, target / 'kernel.elf', raw_path=target / 'styleos.bin', invalid=invalid)
    if site != verified:
        raise ValueError('ownership-site metadata differs from independently checked image proof/SHA-256')
    manifest = json.loads((folder / 'manifest.json').read_text())
    expected_modes = {'invalid_boot_info': False, 'invalid_memory_map': False,
                      'test_exception': False, 'test_bootpool': False,
                      'test_paging': False, 'test_paging_fault': False,
                      'test_ownership_plan': not invalid, 'test_ownership_plan_invalid': invalid}
    if (not isinstance(manifest, dict) or
            any(type(manifest.get(key)) is not bool or manifest[key] != value
                for key, value in expected_modes.items())):
        raise ValueError('ownership manifest mode mismatch')
    artifacts = manifest.get('artifacts')
    if (not isinstance(artifacts, list) or any(not isinstance(row, dict) or
            not isinstance(row.get('path'), str) for row in artifacts)):
        raise ValueError('ownership manifest artifacts are invalid')
    names = (('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')) if arch == 'x86_64' else (
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256'))
    selected = [row for row in artifacts if row['path'].startswith(arch + '/')]
    if len(selected) != 2:
        raise ValueError('ownership manifest needs exactly two artifacts for selected architecture')
    for name, key in names:
        matches = [row for row in selected if row['path'] == arch + '/' + name]
        if (len(matches) != 1 or type(matches[0].get('size')) is not int or
                matches[0]['size'] != (target / name).stat().st_size or
                matches[0].get('sha256') != verified.get(key)):
            raise ValueError('ownership manifest artifact size/SHA-256 mismatch')
    return verified



_TASKS_STATE_FIELDS = ('mode', 'root', 'control0', 'control1', 'flags', 'cs',
                       'idt_base', 'idt_bytes', 'gdt_base', 'gdt_bytes', 'pat',
                       'pa_bits', 'gdt_pages', 'gdt_pa0', 'gdt_pa1')
def _tasks_sources(arch):
    common = ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
              'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
              'kernel/exception.c', 'kernel/boot_pool.c', 'kernel/tasks.c',
              'kernel/tasks_demo.c', f'arch/{arch}/tasks.c',
              f'arch/{arch}/tasks_context.S']
    platform = (['boot/x86_64/exit_boot.c', 'boot/x86_64/uefi.c',
                 'boot/x86_64/memory_map.c', 'arch/x86_64/serial.c',
                 'arch/x86_64/entry.S', 'arch/x86_64/exceptions.c',
                 'arch/x86_64/exception_entry.S'] if arch == 'x86_64' else
                ['boot/aarch64/platform.c', 'boot/aarch64/start.S',
                 'boot/aarch64/fdt_memory.c', 'arch/aarch64/console.c'])
    return common + platform


_TASKS_CODE = ('styleos_tasks_bootstrap', 'styleos_tasks_a', 'styleos_tasks_b',
               'tasks_arch_switch', 'tasks_arch_dispatch', 'tasks_arch_probe', 'tasks_arch_finish',
               'styleos_tasks_probe_resume', 'styleos_tasks_exit_resume',
               'styleos_tasks_trap_return', 'styleos_tasks_dispatch_resume')


def serial_tasks_ok(output, arch, site, ram_mib=128, el2=False):
    """Accept only the bound two-stack, six-dispatch machine-context trace."""
    return tasks_report(output, arch, site, ram_mib, el2) is not None


def tasks_report(output, arch, site, ram_mib=128, el2=False):
    """Return parsed evidence only after every task contract has passed."""
    try:
        return _tasks_report(output, arch, site, ram_mib, el2)
    except (ValueError, KeyError, TypeError, IndexError, OverflowError):
        return None


def _tasks_report(output, arch, site, ram_mib, el2):
    def require(condition):
        if not condition:
            raise ValueError('invalid tasks trace')

    def integer(value):
        return type(value) is int and 0 <= value < 1 << 64

    def inside(inner, outer):
        return outer[0] <= inner[0] < inner[1] <= outer[1]

    def overlaps(one, two):
        return one[0] < two[1] and two[0] < one[1]

    require(isinstance(site, dict) and site.get('arch') == arch and
            arch in ('x86_64', 'aarch64') and type(site.get('page_size')) is int and
            site['page_size'] == 4096 and not any(marker in output.lower() for marker in
            (b'timeout', b'timed out', b'boot error', b'boot_error', b'early exception',
             b'styleos exception', b'styleos emergency')))
    lines = output.splitlines()
    first = next((n for n, line in enumerate(lines) if line.startswith(b'STYLEOS TASKS')), None)
    require(first is not None)
    prefix = b'\n'.join(lines[:first]) + b'\n'
    require(serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False))
    require(arch != 'x86_64' or serial_idt_ready(prefix))
    memory = {}
    for line in lines[:first]:
        match = _RANGE.fullmatch(line)
        if match:
            memory.setdefault(match[1], []).append((int(match[2], 16), int(match[3], 16)))
    image, stack, handoff, source = (memory[key][0] for key in
                                    (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE'))
    require(all(integer(site.get(key)) for key in ('image_start', 'image_end', 'image_size')) and
            site['image_size'] == image[1] - image[0])
    if arch == 'x86_64':
        require(site.get('address_space') == 'image-rva' and site['image_start'] == 0 and
                site['image_end'] == site['image_size'])
        relocation = image[0]
    else:
        require(site.get('address_space') == 'identity-image-address' and
                (site['image_start'], site['image_end']) == image and
                all(integer(site.get(key)) for key in ('bss_start', 'bss_end')) and
                image[0] <= site['bss_start'] < site['bss_end'] <= image[1])
        relocation = 0
    sizes = dict(stack_a=16384, stack_b=16384, control=4096, diagnostic=12400,
                 stack=65536, pool=32768, pool_metadata=4096)
    sizes.update(dict(idt=4096, handoff=96) if arch == 'x86_64' else dict(vectors=2048))
    objects = site.get('objects')
    require(isinstance(objects, dict) and set(objects) == set(sizes))
    expected = {}
    for name, size in sizes.items():
        item = objects[name]
        require(isinstance(item, dict) and all(integer(item.get(key)) for key in
                ('start', 'end', 'bytes')) and item['bytes'] == size and
                item['end'] - item['start'] == size)
        bounds = item['start'] + relocation, item['end'] + relocation
        alignment = dict(stack=16, diagnostic=8, idt=16, handoff=8, vectors=2048).get(name, 4096)
        require(bounds[1] < 1 << 64 and inside(bounds, image) and not bounds[0] % alignment and
                any(inside(bounds, reserve) for reserve in memory[b'RESERVED']) and
                not any(overlaps(bounds, candidate) for candidate in memory[b'CANDIDATE']))
        expected[name] = bounds
    require(expected['stack'] == stack and
            (arch != 'x86_64' or expected['handoff'] == handoff))
    rows = list(expected.values())
    require(not any(overlaps(one, two) for n, one in enumerate(rows) for two in rows[n + 1:]))
    for name in ('stack_a', 'stack_b', 'control'):
        require(not any(overlaps(expected[name], other) for other in (handoff, source)))

    code = site.get('code_sites')
    require(isinstance(code, dict) and set(code) == set(_TASKS_CODE))
    code_pcs = {}
    for name in _TASKS_CODE:
        item = code[name]
        require(isinstance(item, dict) and item.get('symbol') == name and
                all(integer(item.get(key)) for key in ('start', 'end', 'bytes')) and
                item['bytes'] > 0 and item['end'] - item['start'] == item['bytes'] and
                item.get('rva' if arch == 'x86_64' else 'pc') == item['start'])
        raw = bytes.fromhex(item['raw_hex'])
        instruction = bytes.fromhex(item['instruction_hex'])
        require(len(raw) == item['bytes'] and raw.startswith(instruction) and instruction and
                hashlib.sha256(raw).hexdigest() == item.get('sha256') and
                inside((item['start'] + relocation, item['end'] + relocation), image) and
                (arch != 'aarch64' or item['start'] % 4 == 0))
        code_pcs[name] = item['start'] + relocation
    proof = site.get('integer_only')
    require(isinstance(proof, dict) and isinstance(proof.get('compiler_flags'), list) and
            proof['compiler_flags'] and isinstance(proof.get('decoder_version'), str) and
            re.search(r'\bLLVM version \d+', proof['decoder_version']) is not None and
            type(proof.get('decoded_bytes')) is int and proof['decoded_bytes'] > 0 and
            type(proof.get('instruction_count')) is int and proof['instruction_count'] > 0 and
            re.fullmatch(r'[0-9a-f]{64}', proof.get('executable_sha256', '')) is not None and
            isinstance(proof.get('executable_spans'), list) and proof['executable_spans'])

    tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    cursor = 0

    def take(pattern):
        nonlocal cursor
        require(cursor < len(tail))
        match = re.fullmatch(pattern, tail[cursor])
        require(match is not None)
        cursor += 1
        return match

    def fields(label, names):
        nonlocal cursor
        require(cursor < len(tail))
        values = _paging_fields(tail[cursor], b'STYLEOS TASKS ' + label, names)
        require(values is not None)
        cursor += 1
        return values

    def exact(line):
        nonlocal cursor
        require(cursor < len(tail) and tail[cursor] == line)
        cursor += 1

    for name, ident in (('stack_a', b'A'), ('stack_b', b'B')):
        match = take(rb'STYLEOS TASKS STACK id=' + ident + rb' ' + _HEX16 + rb' ' + _HEX16)
        require(tuple(int(value, 16) for value in match.groups()) == expected[name])
    for label, name in ((b'CONTROL', 'control'), (b'MAP_WORKSPACE', 'diagnostic'),
                        (b'DISPATCH_STACK', 'stack')):
        match = take(b'STYLEOS TASKS ' + label + b' ' + _HEX16 + b' ' + _HEX16)
        require(tuple(int(value, 16) for value in match.groups()) == expected[name])
    before = fields(b'STATE', _TASKS_STATE_FIELDS)
    require(32 <= before['pa_bits'] <= (52 if arch == 'x86_64' else 48))
    limit = 1 << before['pa_bits']
    require(all(end <= limit for ranges in memory.values() for _start, end in ranges))
    if arch == 'x86_64':
        require(before['mode'] & 0x80000001 == 0x80000001 and not before['mode'] & 0x60000000 and
                before['root'] and not before['root'] % 4096 and before['root'] + 4096 <= limit and
                before['control0'] & 0x20 and
                not before['control0'] & sum(1 << bit for bit in (12, 17, 21, 22, 23, 24)) and
                before['control1'] & 0xd00 == 0xd00 and before['pat'] & 255 == 6 and
                not before['flags'] & 0x600 and 0 < before['cs'] <= 0xfff8 and not before['cs'] & 7 and
                before['cs'] + 8 <= before['gdt_bytes'] and
                (before['idt_base'], before['idt_base'] + before['idt_bytes']) == expected['idt'] and
                0 < before['gdt_bytes'] <= 4096 and not before['gdt_base'] & 7 and
                before['gdt_base'] + before['gdt_bytes'] < 1 << 47 and
                not overlaps((before['gdt_base'], before['gdt_base'] + before['gdt_bytes']), image) and
                before['gdt_pages'] in (1, 2) and
                lines.count(f'STYLEOS x86_64 IDT ready cs=0x{before["cs"]:016x}'.encode()) == 1)
        require(any(inside((before['root'], before['root'] + 4096), r) for r in memory[b'RESERVED']))
        require(before['gdt_pages'] == ((before['gdt_base'] & 4095) + before['gdt_bytes'] + 4095) // 4096)
        for n in range(2):
            pa = before['gdt_pa' + str(n)]
            require((n >= before['gdt_pages'] and pa == 0) or
                    (n < before['gdt_pages'] and pa and not pa % 4096 and pa + 4096 <= limit and
                     not overlaps((pa, pa + 4096), image) and
                     any(inside((pa, pa + 4096), r) for r in memory[b'RESERVED']) and
                     not any(overlaps((pa, pa + 4096), r) for r in memory[b'CANDIDATE'])))
    else:
        require(before['pa_bits'] in (32, 36, 40, 42, 44, 48) and before['root'] == 0 and
                not before['mode'] & ((1 << 25) | (1 << 24) | (1 << 12) | 5) and
                before['control0'] == 4 and before['control1'] == 1 and before['flags'] == 0x3c0 and
                (before['idt_base'], before['idt_base'] + before['idt_bytes']) == expected['vectors'] and
                all(before[key] == 0 for key in ('cs',
                    'gdt_base', 'gdt_bytes', 'pat', 'gdt_pages', 'gdt_pa0', 'gdt_pa1')))
    created = []
    for ident, name in (('A', 'stack_a'), ('B', 'stack_b')):
        match = take(rb'STYLEOS TASKS CREATE id=' + ident.encode() +
                     rb' state=READY sp=' + _HEX16 + rb' pc=' + _HEX16 + rb' entry=' + _HEX16)
        sp, pc, entry = (int(value, 16) for value in match.groups())
        require(sp == expected[name][1] - (56 if arch == 'x86_64' else 16) and
                pc == code_pcs['styleos_tasks_bootstrap'] and
                entry == code_pcs['styleos_tasks_' + ident.lower()])
        created.append(dict(id=ident, state='READY', sp=sp, pc=pc, entry=entry))
    schedule, local_addresses, dispatcher_address = [], {}, None
    register_count = 8 if arch == 'x86_64' else 11
    full_mask = (1 << register_count) - 1
    for dispatch, (ident, kind) in enumerate((('A', 'START'), ('B', 'START'),
                                            ('A', 'RESUME'), ('B', 'RESUME'),
                                            ('A', 'EXIT'), ('B', 'EXIT')), 1):
        task_stack = expected['stack_' + ident.lower()]
        go = fields(b'GO id=' + ident.encode(), ('count',))
        require(go['count'] == dispatch)
        event = fields(b'EVENT id=' + ident.encode() + b' kind=' + kind.encode(),
                       ('local', 'value', 'regs'))
        require(inside((event['local'], event['local'] + 8),
                       (task_stack[0] + 16, task_stack[1] - 16)) and
                not event['local'] % 8 and
                event['value'] == (0xa11ce55a12345678 if ident == 'A' else 0xb0b55aa987654321) and
                event['regs'] == (0 if kind == 'START' else full_mask))
        require(ident not in local_addresses or local_addresses[ident] == event['local'])
        local_addresses[ident] = event['local']
        seq = (dispatch + 1) // 2
        tag = (0x100 if ident == 'A' else 0x200) + seq
        if kind != 'EXIT':
            yielded = fields(b'YIELD id=' + ident.encode(), ('seq', 'tag'))
            require(yielded == dict(seq=seq, tag=tag))
        else:
            exact(b'STYLEOS TASKS RETURN id=' + ident.encode())
        state = b'DONE' if kind == 'EXIT' else b'READY'
        saved = fields(b'SAVED id=' + ident.encode() + b' state=' + state,
                       ('sp', 'pc', 'seq', 'mask'))
        require(inside((saved['sp'], saved['sp'] + 8),
                       (task_stack[0] + 16, task_stack[1] - 16)) and
                saved['sp'] % 16 == 0 and
                saved['pc'] == code_pcs['styleos_tasks_exit_resume' if kind == 'EXIT'
                                          else 'styleos_tasks_probe_resume'] and
                saved['seq'] == min(seq, 2) and saved['mask'] == (0 if kind == 'EXIT' else full_mask))
        registers = []
        if kind != 'EXIT':
            for index in range(register_count):
                reg = fields(b'REG id=' + ident.encode(), ('seq', 'index', 'value'))
                require(reg == dict(seq=seq, index=index,
                                    value=0x1122000000000000 + (index << 32) + tag))
                registers.append(reg)
        dispatch_state = fields(b'DISPATCH', ('local', 'value', 'sp', 'pc'))
        require(inside((dispatch_state['local'], dispatch_state['local'] + 8), stack) and
                not dispatch_state['local'] % 8 and dispatch_state['value'] == 0xd15ca11c12345678 and
                inside((dispatch_state['sp'], dispatch_state['sp'] + 8), stack) and
                dispatch_state['sp'] % 16 == 0 and
                dispatch_state['pc'] == code_pcs['styleos_tasks_dispatch_resume'] and
                (dispatcher_address is None or dispatcher_address == dispatch_state['local']))
        dispatcher_address = dispatch_state['local']
        schedule.append(dict(id=ident, kind=kind, count=dispatch, event=event,
                             yielded=yielded if kind != 'EXIT' else None,
                             returned=kind == 'EXIT', saved=dict(state=state.decode(), **saved), registers=registers,
                             dispatcher=dispatch_state))
    require(fields(b'EMPTY', ('completed', 'dispatches')) == dict(completed=2, dispatches=6))
    exact(b'STYLEOS TASKS RELEASE id=A state=FREE')
    exact(b'STYLEOS TASKS RELEASE id=B state=FREE')
    canaries = []
    for ident in ('A', 'B'):
        values = fields(b'CANARY id=' + ident.encode(), ('low0', 'low1', 'high0', 'high1'))
        require(values == dict(low0=0x6b6b6b6b6b6b6b6b, low1=0x6b6b6b6b6b6b6b6b,
                               high0=0xa5a5a5a5a5a5a5a5, high1=0xa5a5a5a5a5a5a5a5))
        canaries.append(dict(id=ident, **values))
    exact(b'STYLEOS TASKS CANARY PASS')
    after = fields(b'STATE AFTER', _TASKS_STATE_FIELDS)
    require(all(before[key] == after[key] for key in _TASKS_STATE_FIELDS if key != 'flags') and
            ((before['flags'] ^ after['flags']) & 0x600 == 0 if arch == 'x86_64' else
             before['flags'] == after['flags']))
    exact(b'STYLEOS TASKS PASS')
    exact(b'STYLEOS KERNEL HALT')
    require(cursor == len(tail))
    return dict(arch=arch, created=created, schedule=schedule,
                objects={name: dict(start=a, end=b, bytes=b-a) for name, (a, b) in expected.items()},
                state_before=before, state_after=after, completed=2, dispatches=6, canaries=canaries,
                memory_map={key.decode().lower(): [dict(start=a, end=b) for a, b in spans]
                            for key, spans in memory.items()},
                dispatcher_local=dispatcher_address, released=['A', 'B'])


def verified_tasks_site(folder, arch):
    """Recompute storage, continuation and decoded instruction evidence."""
    target = folder / arch
    manifest = json.loads((folder / 'manifest.json').read_text())
    expected_modes = dict(invalid_boot_info=False, invalid_memory_map=False,
                          test_exception=False, test_bootpool=False,
                          test_paging=False, test_paging_fault=False,
                          test_ownership_plan=False, test_ownership_plan_invalid=False,
                          test_tasks=True)
    if (not isinstance(manifest, dict) or
            any(type(manifest.get(key)) is not bool or manifest[key] != value
                for key, value in expected_modes.items())):
        raise ValueError('tasks manifest mode mismatch')
    flags_by_arch = manifest.get('compiler_flags')
    flags = flags_by_arch.get(arch) if isinstance(flags_by_arch, dict) else None
    if (not isinstance(flags, list) or not flags or
            any(not isinstance(flag, str) or not flag for flag in flags)):
        raise ValueError('tasks manifest actual compiler flags are missing')
    sources_by_arch = manifest.get('task_sources')
    sources = sources_by_arch.get(arch) if isinstance(sources_by_arch, dict) else None
    if (not isinstance(sources, list) or any(not isinstance(source, str) for source in sources) or
            len(sources) != len(_tasks_sources(arch)) or set(sources) != set(_tasks_sources(arch))):
        raise ValueError('tasks manifest source list differs from the dedicated profile')
    expected_target = '--target=' + ('x86_64-pc-win32-coff' if arch == 'x86_64' else 'aarch64-none-elf')
    if (expected_target not in flags or flags.count('-DSTYLEOS_TEST_TASKS=1') != 1 or
            any(flag.startswith('-DSTYLEOS_TEST_') and flag != '-DSTYLEOS_TEST_TASKS=1' for flag in flags)):
        raise ValueError('tasks manifest compiler flags do not select the dedicated target/profile')
    site = json.loads((target / 'tasks-site.json').read_text())
    if arch == 'x86_64':
        verified = describe_tasks(arch, target / 'BOOTX64.EFI', target / 'kernel.map',
                                 compiler_flags=flags)
        verified['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
    elif arch == 'aarch64':
        verified = describe_tasks(arch, target / 'kernel.elf', raw_path=target / 'styleos.bin',
                                 compiler_flags=flags)
    else:
        raise ValueError('unsupported tasks architecture')
    if (not isinstance(verified.get('integer_only'), dict) or
            verified['integer_only'].get('compiler_flags') != flags):
        raise ValueError('tasks compiler flags differ from manifest actual flags')
    if site != verified:
        raise ValueError('tasks-site metadata differs from independently checked image proof/SHA-256')
    artifacts = manifest.get('artifacts')
    if (not isinstance(artifacts, list) or any(not isinstance(row, dict) or
            not isinstance(row.get('path'), str) for row in artifacts)):
        raise ValueError('tasks manifest artifacts are invalid')
    names = (('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')) if arch == 'x86_64' else (
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256'))
    selected = [row for row in artifacts if row['path'].startswith(arch + '/')]
    if len(selected) != 2:
        raise ValueError('tasks manifest needs exactly two artifacts for selected architecture')
    for name, key in names:
        matches = [row for row in selected if row['path'] == arch + '/' + name]
        if (len(matches) != 1 or type(matches[0].get('size')) is not int or
                matches[0]['size'] != (target / name).stat().st_size or
                matches[0].get('sha256') != verified.get(key)):
            raise ValueError('tasks manifest artifact size/SHA-256 mismatch')
    return verified



_USER_MACHINE_FIELDS = ('root', 'mode', 'control0', 'control1', 'control2',
                        'control3', 'vectors', 'gdt', 'tss')
_USER_FRAME_FIELDS = ('vector', 'syndrome', 'address', 'root', 'kernel_sp',
                      'pc', 'sp', 'status', 'cs', 'ss')
_USER_ROOTS = ('SAFE', 'A', 'B')
_USER_VA = {'A': dict(code=0x10000000, stack=0x11000000, data=0x12000000),
            'B': dict(code=0x10000000, stack=0x11010000, data=0x12010000)}


def _user_processes_sources(arch):
    return [source.replace('kernel/tasks.c', 'kernel/user_processes.c')
            .replace('kernel/tasks_demo.c', 'kernel/user_processes_demo.c')
            .replace(f'arch/{arch}/tasks.c', f'arch/{arch}/user_processes.c')
            .replace(f'arch/{arch}/tasks_context.S', f'arch/{arch}/user_processes_entry.S')
            for source in _tasks_sources(arch)] + [
                'kernel/process_runtime.c', 'kernel/process_scheduler.c']


def verified_user_processes_site(folder, arch):
    """Bind the decoded final binary and exact isolated build to its manifest."""
    try:
        from .user_processes_site import describe_user_processes
    except ImportError:
        from user_processes_site import describe_user_processes
    if arch not in ('x86_64', 'aarch64'):
        raise ValueError('unsupported user-process architecture')
    target = folder / arch
    manifest = json.loads((folder / 'manifest.json').read_text())
    modes = dict(invalid_boot_info=False, invalid_memory_map=False,
                 test_exception=False, test_bootpool=False, test_paging=False,
                 test_paging_fault=False, test_ownership_plan=False,
                 test_ownership_plan_invalid=False, test_tasks=False,
                 test_user_processes=True)
    if (not isinstance(manifest, dict) or any(type(manifest.get(key)) is not bool or
            manifest[key] != value for key, value in modes.items())):
        raise ValueError('user-process manifest mode mismatch')
    flag_map = manifest.get('compiler_flags')
    source_map = manifest.get('user_processes_sources')
    if not isinstance(flag_map, dict) or not isinstance(source_map, dict):
        raise ValueError('user-process flags/source architecture maps malformed')
    flags = flag_map.get(arch)
    sources = source_map.get(arch)
    if (not isinstance(flags, list) or not flags or
            any(not isinstance(flag, str) or not flag for flag in flags) or
            not isinstance(sources, list) or any(not isinstance(source, str) for source in sources) or
            len(sources) != len(_user_processes_sources(arch)) or
            set(sources) != set(_user_processes_sources(arch))):
        raise ValueError('user-process flags/source set differs from dedicated profile')
    define = '-DSTYLEOS_TEST_USER_PROCESSES=1'
    target_flag = '--target=' + ('x86_64-pc-win32-coff' if arch == 'x86_64' else 'aarch64-none-elf')
    if (target_flag not in flags or flags.count(define) != 1 or
            any(flag.startswith('-DSTYLEOS_TEST_') and flag != define for flag in flags)):
        raise ValueError('user-process target/profile compiler flags mismatch')
    compiler = json.loads((target / 'user-processes-compiler.json').read_text())
    if compiler != dict(flags=flags, sources=sources):
        raise ValueError('user-process compiler sidecar differs from manifest')
    if arch == 'x86_64':
        verified = describe_user_processes(arch, target / 'BOOTX64.EFI',
            target / 'kernel.map', compiler_flags=flags)
        verified['disk_sha256'] = hashlib.sha256((target / 'styleos.img').read_bytes()).hexdigest()
    else:
        verified = describe_user_processes(arch, target / 'kernel.elf',
            raw_path=target / 'styleos.bin', compiler_flags=flags)
    if (verified.get('integer_only', {}).get('compiler_flags') != flags or
            json.loads((target / 'user-processes-site.json').read_text()) != verified):
        raise ValueError('user-process site differs from independently decoded final image')
    artifacts = manifest.get('artifacts')
    if (not isinstance(artifacts, list) or any(not isinstance(row, dict) or
            not isinstance(row.get('path'), str) for row in artifacts)):
        raise ValueError('user-process manifest artifacts malformed')
    names = (('BOOTX64.EFI', 'binary_sha256'), ('styleos.img', 'disk_sha256')) if arch == 'x86_64' else (
        ('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256'))
    selected = [row for row in artifacts if row['path'].startswith(arch + '/')]
    if len(selected) != 2:
        raise ValueError('user-process manifest needs exactly two selected artifacts')
    for name, key in names:
        rows = [row for row in selected if row['path'] == arch + '/' + name]
        if (len(rows) != 1 or type(rows[0].get('size')) is not int or
                rows[0]['size'] != (target / name).stat().st_size or
                rows[0].get('sha256') != verified.get(key)):
            raise ValueError('user-process artifact size/SHA256 mismatch')
    return verified


def _user_table_report(arch, image, objects, roots, records):
    """Independently traverse all nonzero raw descriptors of all three roots.

    Exact leaf-set equality rejects extra aliases, huge leaves, unknown table
    pages, foreign private pages and user access to kernel metadata. The guest
    records every nonzero slot of every admitted table page, not only a selected
    VA walk or permission summary.
    """
    def need(condition):
        if not condition:
            raise ValueError('invalid live user tables')

    arena = objects['tables']
    need(arena[1] - arena[0] == 3 * 32 * 4096 and arena[0] % 4096 == 0)
    result = {}
    physical_mask = ((1 << 48) - 1) & ~4095
    for number, ident in enumerate(_USER_ROOTS):
        root = roots[ident]
        base = arena[0] + number * 32 * 4096
        need(root['pa'] == base and 1 <= root['pages'] <= 32)
        rows = records[ident]
        need(rows == sorted(rows, key=lambda item: (item['table'], item['index'])))
        slots = {}
        for row in rows:
            table, index, entry = row['table'], row['index'], row['entry']
            need(base <= table < base + root['pages'] * 4096 and table % 4096 == 0 and
                 0 <= index < 512 and 0 < entry < 1 << 64 and (table, index) not in slots)
            slots[table, index] = entry
        visited, leaves = set(), {}

        def walk(table, level, prefix, writable=True, user=True, executable=True):
            need(table not in visited and base <= table < base + root['pages'] * 4096)
            visited.add(table)
            for index in range(512):
                entry = slots.get((table, index), 0)
                if not entry:
                    continue
                pa = entry & physical_mask
                va = prefix | index << (39 - 9 * level)
                if level < 3:
                    if arch == 'x86_64':
                        need(entry & 1 and not entry & ~(physical_mask | 0x27))
                        walk(pa, level + 1, va, writable and bool(entry & 2),
                             user and bool(entry & 4), executable)
                    else:
                        need(entry & 3 == 3 and not entry & ~(physical_mask | 3))
                        walk(pa, level + 1, va)
                    continue
                need(va not in leaves)
                if arch == 'x86_64':
                    need(entry & 1 and not entry & ~(physical_mask | (1 << 63) | 0x67))
                    leaves[va] = dict(pa=pa, writable=writable and bool(entry & 2),
                        user=user and bool(entry & 4), executable=executable and not bool(entry >> 63),
                        flags=entry & ~physical_mask)
                else:
                    need(entry & 3 == 3 and not entry & ~(physical_mask | (3 << 53) | 0x7df) and
                         entry & (1 << 10))
                    leaves[va] = dict(pa=pa, writable=not bool(entry & (1 << 7)),
                        user=bool(entry & (1 << 6)), executable=not bool(entry & (1 << 54)),
                        kernel_executable=not bool(entry & (1 << 53)),
                        flags=entry & ~physical_mask)
        walk(root['pa'], 0, 0)
        need(visited == set(range(base, base + root['pages'] * 4096, 4096)))
        need(set(table for table, _index in slots) <= visited)
        expected = {}
        for va in range(image[0] & ~4095, (image[1] + 4095) & ~4095, 4096):
            expected[va] = dict(pa=va, writable=True, user=False, executable=True)
        if arch == 'aarch64':
            expected[0x9000000] = dict(pa=0x9000000, writable=True, user=False, executable=False)
        if ident != 'SAFE':
            for kind in ('code', 'stack', 'data'):
                va = _USER_VA[ident][kind]
                need(va not in expected)
                expected[va] = dict(pa=objects[kind + '_' + ident.lower()][0],
                    writable=kind != 'code', user=True, executable=kind == 'code')
        need(set(leaves) == set(expected))
        for va, want in expected.items():
            actual = leaves[va]
            for key in ('pa', 'writable', 'user'):
                need(actual[key] == want[key])
            if arch == 'x86_64':
                need(actual['executable'] == want['executable'])
            else:
                # UXN protects every kernel-only mapping even when EL1 code
                # remains executable. PXN forbids EL1 fetch from user aliases.
                need(actual['executable'] == (want['executable'] and want['user']))
                need(actual['kernel_executable'] == (want['executable'] and not want['user']))
                expected_flags = (3 | (3 << 8) | (1 << 10) | (1 << 54))
                if va == 0x9000000:
                    expected_flags = 3 | 4 | (2 << 8) | (1 << 10) | (3 << 53)
                elif want['user']:
                    expected_flags = 3 | (3 << 8) | (1 << 10) | (1 << 53)
                    expected_flags |= (3 << 6) if not want['writable'] else ((1 << 6) | (1 << 54))
                need(actual['flags'] == expected_flags)
        result[ident] = dict(root=root['pa'], pages=root['pages'],
            descriptors=rows, leaves=[dict(va=va, **leaves[va]) for va in sorted(leaves)])
    return result


def serial_user_processes_ok(output, arch, site, ram_mib=128, el2=False):
    return user_processes_report(output, arch, site, ram_mib, el2) is not None


def user_processes_report(output, arch, site, ram_mib=128, el2=False):
    try:
        return _user_processes_report(output, arch, site, ram_mib, el2)
    except (ValueError, KeyError, TypeError, IndexError, OverflowError):
        return None


def _user_processes_report(output, arch, site, ram_mib, el2):
    def need(condition):
        if not condition:
            raise ValueError('invalid user-process trace')
    def integer(x):
        return type(x) is int and 0 <= x < 1 << 64
    def inside(one, two):
        return two[0] <= one[0] < one[1] <= two[1]
    def overlap(one, two):
        return one[0] < two[1] and two[0] < one[1]
    need(arch in ('x86_64', 'aarch64') and isinstance(site, dict) and site.get('arch') == arch and
         site.get('page_size') == 4096 and not any(marker in output.lower() for marker in
         (b'timeout', b'timed out', b'boot error', b'boot_error', b'early exception',
          b'styleos exception', b'styleos emergency', b'styleos user error')))
    lines = output.splitlines()
    first = next((n for n, line in enumerate(lines) if line.startswith(b'STYLEOS USER')), None)
    need(first is not None)
    prefix = b'\n'.join(lines[:first]) + b'\n'
    need(serial_memory_ok(prefix, arch, 'normal', ram_mib, el2, terminal=False) and
         (arch != 'x86_64' or serial_idt_ready(prefix)))
    memory = {}
    for line in lines[:first]:
        match = _RANGE.fullmatch(line)
        if match:
            memory.setdefault(match[1], []).append((int(match[2], 16), int(match[3], 16)))
    image, stack, handoff, source = (memory[key][0] for key in
                                    (b'IMAGE', b'STACK', b'HANDOFF', b'SOURCE'))
    need(all(integer(site.get(key)) for key in ('image_start', 'image_end', 'image_size')) and
         site['image_size'] == image[1] - image[0])
    relocation = image[0] if arch == 'x86_64' else 0
    need((site['image_start'], site['image_end']) ==
         ((0, image[1] - image[0]) if arch == 'x86_64' else image) and
         site.get('address_space') == ('image-rva' if arch == 'x86_64' else 'identity-image-address'))
    sizes = dict(tables=3*32*4096, control=16384, entry_stack=16384,
                 stack_a=4096, stack_b=4096, data_a=4096, data_b=4096,
                 code_a=4096, code_b=4096, diagnostic=12400, stack=65536,
                 pool=32768, pool_metadata=4096)
    sizes.update(dict(idt=4096, handoff=96, gdt=4096, tss=4096, user_idt=4096)
                 if arch == 'x86_64' else dict(vectors=2048, user_vectors=2048))
    objects = site.get('objects')
    need(isinstance(objects, dict) and set(objects) == set(sizes))
    expected = {}
    for name, size in sizes.items():
        item = objects[name]
        need(isinstance(item, dict) and all(integer(item.get(key)) for key in ('start', 'end', 'bytes')) and
             item['bytes'] == size and item['end'] - item['start'] == size)
        bounds = item['start'] + relocation, item['end'] + relocation
        alignment = dict(stack=16, diagnostic=8, idt=16, handoff=8,
                         vectors=2048, user_vectors=2048).get(name, 4096)
        need(bounds[0] % alignment == 0 and bounds[1] < 1 << 64 and inside(bounds, image) and
             any(inside(bounds, span) for span in memory[b'RESERVED']) and
             not any(overlap(bounds, span) for span in memory[b'CANDIDATE']))
        expected[name] = bounds
    need(expected['stack'] == stack and (arch != 'x86_64' or expected['handoff'] == handoff))
    need(not any(overlap(a, b) for i, a in enumerate(expected.values())
                 for b in list(expected.values())[i+1:]))
    for name in ('tables', 'control', 'entry_stack', 'stack_a', 'stack_b', 'data_a', 'data_b',
                 'code_a', 'code_b'):
        need(not any(overlap(expected[name], other) for other in (handoff, source)))
    proof = site.get('integer_only')
    need(isinstance(proof, dict) and isinstance(proof.get('compiler_flags'), list) and
         proof['compiler_flags'] and isinstance(proof.get('decoder_version'), str) and
         re.search(r'\bLLVM version \d+', proof['decoder_version']) is not None and
         type(proof.get('decoded_bytes')) is int and proof['decoded_bytes'] > 0 and
         type(proof.get('instruction_count')) is int and proof['instruction_count'] > 0 and
         re.fullmatch(r'[0-9a-f]{64}', proof.get('executable_sha256', '')) is not None)
    programs = site.get('programs')
    need(isinstance(programs, dict) and set(programs) == {'A', 'B'} and site.get('fault_va') == 0x12010000)
    pcs = {}
    for ident in ('A', 'B'):
        program = programs[ident]
        need(program.get('code_object') == 'code_' + ident.lower() and
             all(program.get(kind + '_va') == va for kind, va in _USER_VA[ident].items()))
        sites = program.get('sites')
        page_bytes = bytes.fromhex(program['raw_hex'])
        need(len(page_bytes) == 4096 and hashlib.sha256(page_bytes).hexdigest() == program.get('sha256'))
        names = ('entry', 'yield_resume', 'fault') if ident == 'A' else ('entry', 'yield_resume', 'exit', 'exit_resume')
        need(isinstance(sites, dict) and all(name in sites for name in names))
        pcs[ident] = {}
        for name in names:
            item = sites[name]
            pc = item.get('user_pc')
            raw = bytes.fromhex(item['raw_hex'])
            image_pc = item.get('image_rva' if arch == 'x86_64' else 'image_pc')
            need(integer(pc) and integer(image_pc) and raw and
                 hashlib.sha256(raw).hexdigest() == item.get('sha256') and
                 pc == 0x10000000 + image_pc + relocation - expected['code_' + ident.lower()][0] and
                 0x10000000 <= pc < pc + len(raw) <= 0x10001000 and
                 page_bytes[pc - 0x10000000:pc - 0x10000000 + len(raw)] == raw)
            pcs[ident][name] = pc
        need(pcs[ident]['entry'] == 0x10000000)
        call = sites['yield_resume']['call_site']
        raw = bytes.fromhex(call['raw_hex'])
        need(call['user_pc'] + (2 if arch == 'x86_64' else 4) == pcs[ident]['yield_resume'] and
             raw.hex() == ('cd80' if arch == 'x86_64' else '210000d4') and
             hashlib.sha256(raw).hexdigest() == call.get('sha256') and
             page_bytes[call['user_pc'] - 0x10000000:call['user_pc'] - 0x10000000 + len(raw)] == raw)
    need(pcs['B']['exit_resume'] == pcs['B']['exit'] + (2 if arch == 'x86_64' else 4))
    need(programs['A']['sites']['fault']['raw_hex'].startswith('488b07' if arch == 'x86_64' else '010040f9'))
    tail = [line for line in lines[first:] if line.startswith(b'STYLEOS ')]
    cursor = 0
    def take(pattern):
        nonlocal cursor
        need(cursor < len(tail))
        match = re.fullmatch(pattern, tail[cursor]);need(match is not None);cursor += 1
        return match
    def fields(label, names):
        nonlocal cursor
        need(cursor < len(tail))
        values = _paging_fields(tail[cursor], b'STYLEOS USER ' + label, names)
        need(values is not None);cursor += 1
        return values
    def exact(line):
        nonlocal cursor
        need(cursor < len(tail) and tail[cursor] == line);cursor += 1
    logged = {}
    while cursor < len(tail) and tail[cursor].startswith(b'STYLEOS USER OBJECT '):
        m = take(rb'STYLEOS USER OBJECT name=([a-z_]+) ' + _HEX16 + rb' ' + _HEX16)
        name=m[1].decode();need(name not in logged)
        logged[name] = (int(m[2],16),int(m[3],16))
    need(logged == expected)
    roots, entries = {}, {}
    for ident in _USER_ROOTS:
        roots[ident] = fields(('ROOT id=' + ident).encode(), ('pa', 'pages'))
        rows=[]
        while cursor < len(tail) and tail[cursor].startswith(('STYLEOS USER TABLE id=' + ident + ' ').encode()):
            rows.append(fields(('TABLE id=' + ident).encode(), ('table', 'index', 'entry')))
        end = fields(('TABLES END id=' + ident).encode(), ('entries',))
        need(end['entries'] == len(rows));entries[ident] = rows
    table_proof = _user_table_report(arch, image, expected, roots, entries)
    before = fields(b'MACHINE phase=SAFE', _USER_MACHINE_FIELDS)
    need(before['root'] == roots['SAFE']['pa'])
    if arch == 'x86_64':
        need(before['mode'] & 0x80010001 == 0x80010001 and before['control0'] & (1 << 5) and
             not before['control0'] & ((1 << 12) | (1 << 17)) and
             before['control1'] & 0xd00 == 0xd00 and before['control2'] & 255 == 6 and
             before['control3'] == 8 and before['vectors'] == expected['user_idt'][0] and
             before['gdt'] == expected['gdt'][0] and before['tss'] == 0x28)
    else:
        tcr = before['control0'];ips = tcr >> 32 & 7
        need(ips <= 5 and tcr == 16 | (3 << 12) | (16 << 16) | (1 << 23) | (2 << 30) | (ips << 32))
        need(before['mode'] == 0x30d00801 and before['control1'] == 0x44 and
             before['control2'] == 4 and before['control3'] & 0x3c0 == 0x3c0 and
             before['vectors'] == expected['user_vectors'][0] and before['gdt'] == before['tss'] == 0)
    schedule, saved = [], {}
    dispatcher_sp = None
    for count, (ident, kind) in enumerate((('A','YIELD'),('B','YIELD'),('A','FAULT'),('B','EXIT')), 1):
        go = fields(('GO id=' + ident).encode(), ('count','root','pc','sp'))
        initial_sp = _USER_VA[ident]['stack'] + 4096
        want_sp = initial_sp if count <= 2 else initial_sp - 16
        want_pc = pcs[ident]['entry'] if count <= 2 else pcs[ident]['yield_resume']
        need(go == dict(count=count, root=roots[ident]['pa'],pc=want_pc,sp=want_sp))
        frame = fields(('FRAME id=' + ident + ' kind=' + kind).encode(), _USER_FRAME_FIELDS)
        expected_pc = pcs[ident]['yield_resume'] if kind == 'YIELD' else (
            pcs['A']['fault'] if kind == 'FAULT' else pcs['B']['exit_resume'])
        need(frame['root'] == roots[ident]['pa'] and frame['pc'] == expected_pc and
             frame['sp'] == initial_sp - 16 and frame['sp'] % 16 == 0 and
             frame['kernel_sp'] % 16 == 0 and
             inside((frame['kernel_sp'], frame['kernel_sp'] +
                     (176 if arch == 'x86_64' else 320)), expected['entry_stack']))
        if arch == 'x86_64':
            need(frame['cs'] == 0x23 and frame['ss'] == 0x1b and frame['status'] & 2 and
                 not frame['status'] & ((3 << 12) | (1 << 9) | (1 << 10) | (1 << 14) | (1 << 17)))
            need(frame['vector'] == (14 if kind == 'FAULT' else 128) and
                 frame['syndrome'] == (4 if kind == 'FAULT' else 0) and
                 frame['address'] == (0x12010000 if kind == 'FAULT' else 0))
        else:
            need(frame['cs'] == frame['ss'] == 0 and frame['status'] & 0x3df == 0x3c0 and
                 not frame['status'] & ~((15 << 28) | 0x3c0) and frame['vector'] == 8 and
                 frame['syndrome'] == (0x92000007 if kind == 'FAULT' else
                                      0x56000001 if kind == 'YIELD' else 0x56000002) and
                 frame['address'] == (0x12010000 if kind == 'FAULT' else 0))
        registers=[]
        for index in range(31):
            row=fields(('REG id=' + ident).encode(),('index','value'));need(row['index']==index)
            registers.append(row['value'])
        if arch == 'x86_64':
            need(registers[1] == (0xa11c if ident == 'A' else 0xb22c) and
                 registers[11] == (0xa11ce55a12345678 if ident == 'A' else 0x123456789abcdef0) and
                 registers[15:] == [0]*16)
            if kind != 'FAULT':need(registers[0] == (2 if kind == 'EXIT' else 1))
            else:need(registers[5] == 0x12010000)
        else:
            need(registers[19] == (0xa11c if ident == 'A' else 0xb22c) and
                 registers[20] == (0xa11ce55a12345678 if ident == 'A' else 0x123456789abcdef0))
            if kind == 'FAULT':need(registers[0] == 0x12010000)
            if kind == 'EXIT':need(registers[0] == 2)
        if ident == 'B':
            # This is volatile R10 on x86 / X9 on ARM: an exception frame
            # must preserve it even though T-P1.6's C-call context need not.
            need(registers[9] == 0x55aa001122334455)
        data = {slot:fields(('DATA id='+slot).encode(),('value',))['value'] for slot in ('A','B')}
        need(data['A'] == 0 and data['B'] == (0 if count == 1 else
             0xfedcba9876543210 if count == 4 else 0x123456789abcdef0))
        safe = fields(b'SAFE',('root','sp'))
        need(safe['root'] == roots['SAFE']['pa'] and inside((safe['sp'],safe['sp']+8),stack) and
             safe['sp'] % 16 == 0 and (dispatcher_sp is None or safe['sp']==dispatcher_sp))
        dispatcher_sp=safe['sp']
        if kind=='FAULT':exact(b'STYLEOS USER TERMINATED id=A state=FAULTED')
        schedule.append(dict(id=ident,kind=kind,go=go,frame=frame,registers=registers,data=data,safe=safe))
        saved[ident]=frame
    empty=fields(b'EMPTY',('faulted','exited','dispatches'))
    need(empty==dict(faulted=1,exited=1,dispatches=4))
    for ident in ('A', 'B'):
        release = fields(('RELEASE id=' + ident + ' state=FREE').encode(), ('root',))
        need(release['root'] == roots['SAFE']['pa'])
    after=fields(b'MACHINE phase=FINAL',_USER_MACHINE_FIELDS)
    need(after==before)
    exact(b'STYLEOS USER PASS');exact(b'STYLEOS KERNEL HALT');need(cursor==len(tail))
    return dict(arch=arch,objects={name:dict(start=a,end=b,bytes=b-a) for name,(a,b) in expected.items()},
                roots=table_proof, machine_before=before,machine_after=after,schedule=schedule,
                faulted=['A'],exited=['B'],released=['A','B'],dispatches=4,dispatcher_sp=dispatcher_sp,
                memory_map={key.decode().lower():[dict(start=a,end=b) for a,b in spans]
                            for key,spans in memory.items()})

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
            ('host-ownership', ['tests/test_ownership.c', 'kernel/ownership.c',
                                'kernel/boot_memory.c']),
            ('host-tasks', ['tests/test_tasks.c', 'kernel/tasks.c']),
            ('host-user-processes', ['tests/test_user_processes.c',
                                     'kernel/user_processes.c']),
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
            if name == 'host-ownership':
                x86 = folder / 'test-ownership-x86'
                commands += [[cc, *flags, 'tests/test_ownership_x86.c', '-o', x86], [x86]]
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
                        'paging-fault': '--test-paging-fault',
                        'ownership': '--test-ownership-plan',
                        'ownership-invalid': '--test-ownership-plan-invalid',
                        'tasks': '--test-tasks',
                        'user-processes': '--test-user-processes'}[mode])
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
                   'paging-fault': self.output / 'paging-fault',
                   'ownership': self.output / 'ownership',
                   'ownership-invalid': self.output / 'ownership-invalid',
                   'tasks': self.output / 'tasks',
                   'user-processes': self.output / 'user-processes'}
        names = ('build-for-qemu', 'build-negative', 'build-negative-memory',
                 'build-exception', 'build-bootpool', 'build-paging',
                 'build-paging-fault', 'build-ownership', 'build-ownership-invalid',
                 'build-tasks', 'build-user-processes')
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
                for mode in ('ownership', 'ownership-invalid', 'tasks', 'user-processes'):
                    jobs.append((f'qemu-aarch64-{mode}-el2', arch, binary, folders[mode],
                                 None, mode, True, 128, timeout))
                jobs.append(('qemu-aarch64-ownership-256m', arch, binary, folders['ownership'],
                             None, 'ownership', False, 256, timeout))
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
                    'ownership': b'STYLEOS KERNEL HALT',
                    'ownership-invalid': b'STYLEOS OWNERSHIP ERROR: required',
                    'tasks': b'STYLEOS KERNEL HALT',
                    'user-processes': b'STYLEOS KERNEL HALT',
                    'normal': b'STYLEOS KERNEL HALT'}[mode]
        output = bytearray()
        site = None
        started = time.monotonic()
        process = None
        terminal_seen_at = None
        settled = False
        natural_exit = False
        requested_stop = False
        forced_stop = False
        process_error = None
        selector = selectors.DefaultSelector()

        def result(status, detail, text):
            if mode == 'user-processes':
                try:
                    manifest_sha = hashlib.sha256((folder / 'manifest.json').read_bytes()).hexdigest()
                except OSError:
                    manifest_sha = None
                payload = {
                    'arch': arch, 'mode': mode, 'initial_el2': el2, 'ram_mib': mib,
                    'status': status, 'uart_sha256': hashlib.sha256(output).hexdigest(),
                    'manifest_sha256': manifest_sha, 'user_processes_site': site,
                    'user_processes_trace': user_processes_report(output, arch, site, mib, el2)
                        if status == 'PASS' else None,
                    'requested_stop': requested_stop, 'forced_stop': forced_stop,
                    'natural_exit': natural_exit, 'settled': settled,
                    'returncode': process.returncode if process is not None else None,
                }
                (self.logs / (name + '.json')).write_text(json.dumps(payload, indent=2) + '\n')
            if mode == 'tasks':
                try:
                    manifest_sha = hashlib.sha256((folder / 'manifest.json').read_bytes()).hexdigest()
                except OSError:
                    manifest_sha = None
                payload = {
                    'arch': arch, 'mode': mode, 'initial_el2': el2, 'ram_mib': mib,
                    'status': status, 'uart_sha256': hashlib.sha256(output).hexdigest(),
                    'manifest_sha256': manifest_sha, 'tasks_site': site,
                    'tasks_trace': tasks_report(output, arch, site, mib, el2)
                        if status == 'PASS' else None,
                    'requested_stop': requested_stop, 'forced_stop': forced_stop,
                    'natural_exit': natural_exit, 'settled': settled,
                    'returncode': process.returncode if process is not None else None,
                }
                (self.logs / (name + '.json')).write_text(json.dumps(payload, indent=2) + '\n')
            if mode in ('ownership', 'ownership-invalid'):
                try:
                    manifest_sha = hashlib.sha256((folder / 'manifest.json').read_bytes()).hexdigest()
                except OSError:
                    manifest_sha = None
                payload = {
                    'arch': arch, 'mode': mode, 'initial_el2': el2, 'ram_mib': mib,
                    'status': status, 'uart_sha256': hashlib.sha256(output).hexdigest(),
                    'manifest_sha256': manifest_sha, 'ownership_site': site,
                    'ownership_plan': ownership_report(output, arch)
                        if status == 'PASS' and mode == 'ownership' else None,
                    'refusal': 'required' if status == 'PASS' and mode == 'ownership-invalid' else None,
                    'requested_stop': requested_stop, 'forced_stop': forced_stop,
                    'natural_exit': natural_exit, 'settled': settled,
                    'returncode': process.returncode if process is not None else None,
                }
                (self.logs / (name + '.json')).write_text(json.dumps(payload, indent=2) + '\n')
            return name, status, detail, text

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
            process_error = error
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
                 f'natural_exit={natural_exit} returncode={process.returncode if process else None} settled={settled}\n')
        if process_error is not None:
            return result('FAIL', str(process_error), text)
        if expected not in output:
            return result('FAIL', 'expected serial marker missing / timeout', text)
        if natural_exit or not requested_stop:
            return result('FAIL', 'QEMU exited/reset before intentional harness stop', text)
        # QEMU may handle SIGTERM and return 0; an unrelated error/abort is not OK.
        if process.returncode not in ((0, -9) if forced_stop else (0, -15)):
            return result('FAIL', 'unexpected QEMU process exit status', text)
        if not settled:
            return result('FAIL', 'terminal marker observation window incomplete', text)
        if mode == 'exception':
            try:
                site = json.loads((folder / arch / 'fault-site.json').read_text())
                executable = folder / arch / ('BOOTX64.EFI' if arch == 'x86_64' else 'kernel.elf')
                loaded = folder / arch / ('styleos.img' if arch == 'x86_64' else 'styleos.bin')
                loaded_key = 'disk_sha256' if arch == 'x86_64' else 'raw_image_sha256'
                if (hashlib.sha256(executable.read_bytes()).hexdigest() != site.get('binary_sha256') or
                        hashlib.sha256(loaded.read_bytes()).hexdigest() != site.get(loaded_key)):
                    return result('FAIL', 'fault-site metadata/image SHA-256 mismatch', text)
            except (OSError, ValueError) as error:
                return result('FAIL', 'fault-site metadata unavailable: ' + str(error), text)
            good = serial_exception_ok(output, arch, site, mib, el2)
        elif mode == 'bootpool':
            try:
                site = verified_pool_site(folder, arch)
            except (OSError, ValueError) as error:
                return result('FAIL', 'pool-site proof unavailable/mismatched: ' + str(error), text)
            good = serial_pool_ok(output, arch, site, mib, el2)
        elif mode in ('paging', 'paging-fault'):
            try:
                site = verified_paging_site(folder, arch, mode == 'paging-fault')
            except (OSError, ValueError) as error:
                return result('FAIL', 'paging-site proof unavailable/mismatched: ' + str(error), text)
            good = serial_paging_ok(output, arch, site, mode == 'paging-fault', mib, el2)
        elif mode == 'tasks':
            try:
                site = verified_tasks_site(folder, arch)
            except (OSError, ValueError) as error:
                return result('FAIL', 'tasks-site proof unavailable/mismatched: ' + str(error), text)
            good = serial_tasks_ok(output, arch, site, mib, el2)
        elif mode == 'user-processes':
            try:
                site = verified_user_processes_site(folder, arch)
            except (OSError, ValueError) as error:
                return result('FAIL', 'user-processes-site proof unavailable/mismatched: ' + str(error), text)
            good = serial_user_processes_ok(output, arch, site, mib, el2)
        elif mode in ('ownership', 'ownership-invalid'):
            try:
                site = verified_ownership_site(folder, arch, mode == 'ownership-invalid')
            except (OSError, ValueError) as error:
                return result('FAIL', 'ownership-site proof unavailable/mismatched: ' + str(error), text)
            good = serial_ownership_ok(output, arch, site, mode == 'ownership-invalid', mib, el2)
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
        return result('PASS' if good else 'FAIL', 'UART handoff/memory/exception/pool/paging/ownership/tasks/user-processes contract checked; QEMU stopped by harness', text)


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
