#!/usr/bin/env python3
"""Build and run the ARM-only external disk application experiment.

The only host disk writer is the explicitly scoped fixture packer, before any
VM starts. It creates new regular 1-MiB images in a fresh run directory, using
the unchanged T-P1.8A store implementation. Every VM subsequently reads only.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import selectors
import shutil
import signal
import struct
import subprocess
import sys
import time
import zlib

try:
    from .test_storage import (ROOT, DISK_BYTES, write_new, json_new,
        new_run_directory, disk_state, source_snapshot as storage_source_snapshot, tool_versions)
except ImportError:
    from test_storage import (ROOT, DISK_BYTES, write_new, json_new,
        new_run_directory, disk_state, source_snapshot as storage_source_snapshot, tool_versions)

STAGES = (('variant-a', 'a', 'run'), ('variant-b', 'b', 'run'),
          ('reject', 'invalid', 'reject'))
FILE_NAME = b'app.bin'
HEX64 = rb'0x([0-9a-f]{16})'
CODE_VA = 0x10000000
STACK_VA = 0x11000000
FAULT_VA = 0x12010000
PAGE = 4096
MODES = ('invalid_boot_info', 'invalid_memory_map', 'test_exception',
         'test_bootpool', 'test_paging', 'test_paging_fault',
         'test_ownership_plan', 'test_ownership_plan_invalid',
         'test_tasks', 'test_user_processes', 'test_storage')


def qemu_command(qemu, image, disk, phase):
    if phase not in ('run', 'reject'):
        raise ValueError('unknown fixed disk-app phase')
    file_node = dict(driver='file', filename=str(Path(disk).resolve(strict=True)),
        **{'node-name': 'app-file', 'cache': {'direct': False, 'no-flush': False}})
    # The unchanged 1.8A driver rejects VIRTIO_BLK_F_RO. Keep its negotiated
    # backend contract, then prove the new guest path performs READ only.
    raw_node = {'driver': 'raw', 'file': 'app-file', 'node-name': 'app',
                'read-only': False}
    return [str(qemu), '-machine', 'virt', '-cpu', 'cortex-a57', '-smp', '1',
        '-m', '128M', '-accel', 'tcg,thread=single', '-display', 'none',
        '-serial', 'stdio', '-monitor', 'none', '-net', 'none', '-no-reboot',
        '-global', 'virtio-mmio.force-legacy=false', '-kernel',
        str(Path(image).resolve(strict=True)), '-append', 'styleos.diskapp=' + phase,
        '-blockdev', json.dumps(file_node, separators=(',', ':')),
        '-blockdev', json.dumps(raw_node, separators=(',', ':')),
        '-device', 'virtio-blk-device,drive=app,id=app0']


def fixture_disk(run, name, app_file, packer):
    """Delegate *new-file-only* pre-VM fixture writing to the C store packer."""
    if name not in ('variant-a', 'variant-b', 'reject'):
        raise ValueError('unknown fixed fixture name')
    run = Path(run).resolve(strict=True)
    directory = run / 'disks'
    if directory.is_symlink() or directory.resolve(strict=True).parent != run:
        raise ValueError('disk directory escaped the fresh run')
    path = directory / (name + '.img')
    result = subprocess.run([str(packer), str(app_file), str(path)], cwd=ROOT,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
    if result.returncode:
        raise ValueError('fixture packer refused: ' + result.stdout.strip())
    write_new(run / 'test-results' / ('fixture-' + name + '.log'),
        '$ ' + ' '.join(map(str, (packer, app_file, path))) + '\n' + result.stdout)
    return path, disk_state(path)


def disk_app_sources():
    return ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
        'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
        'kernel/exception.c', 'kernel/boot_pool.c', 'kernel/storage.c',
        'arch/aarch64/virtio_block.c', 'kernel/disk_app.c', 'kernel/disk_app_demo.c',
        'boot/aarch64/disk_app_fdt.c',
        'arch/aarch64/disk_app.c', 'arch/aarch64/disk_app_entry.S',
        'boot/aarch64/platform.c', 'boot/aarch64/start.S',
        'boot/aarch64/fdt_memory.c', 'arch/aarch64/console.c']


def verified_disk_app_site(folder):
    try:
        from .disk_app_site import describe_disk_app
    except ImportError:
        from disk_app_site import describe_disk_app
    folder = Path(folder)
    target = folder / 'aarch64'
    manifest = json.loads((folder / 'manifest.json').read_text())
    if (not isinstance(manifest, dict) or manifest.get('test_disk_app') is not True or
            any(manifest.get(key) is not False for key in MODES)):
        raise ValueError('manifest is not the isolated disk-app profile')
    flag_map, source_map = manifest.get('compiler_flags'), manifest.get('disk_app_sources')
    if (not isinstance(flag_map, dict) or not isinstance(source_map, dict) or
            set(flag_map) != {'aarch64'} or set(source_map) != {'aarch64'}):
        raise ValueError('disk-app architecture metadata is malformed')
    flags, sources = flag_map['aarch64'], source_map['aarch64']
    if (not isinstance(flags, list) or not flags or
            any(not isinstance(x, str) or not x for x in flags) or
            sources != disk_app_sources()):
        raise ValueError('disk-app compiler flags/source list mismatch')
    define = '-DSTYLEOS_TEST_DISK_APP=1'
    if (flags.count(define) != 1 or flags.count('--target=aarch64-none-elf') != 1 or
            flags.count('-DSTYLEOS_EXPECT_ARCH=2') != 1 or
            any((flag.startswith(('--target=', '-target')) and
                 flag != '--target=aarch64-none-elf') or
                (flag.startswith('-DSTYLEOS_EXPECT_ARCH') and
                 flag != '-DSTYLEOS_EXPECT_ARCH=2') or
                flag.startswith('-USTYLEOS_') or
                (flag.startswith('-DSTYLEOS_TEST_') and flag != define)
                for flag in flags)):
        raise ValueError('disk-app target/test compiler flags mismatch')
    if json.loads((target / 'disk-app-compiler.json').read_text()) != dict(flags=flags, sources=sources):
        raise ValueError('compiler sidecar differs from manifest')
    site = describe_disk_app('aarch64', target / 'kernel.elf',
        raw_path=target / 'styleos.bin', compiler_flags=flags)
    if (site.get('compiler_flags') != flags or
            json.loads((target / 'disk-app-site.json').read_text()) != site):
        raise ValueError('site differs from independently verified ELF/Image')
    artifacts = manifest.get('artifacts')
    if not isinstance(artifacts, list) or len(artifacts) != 2:
        raise ValueError('disk-app manifest needs exactly two ARM artifacts')
    for name, key in [('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')]:
        rows = [row for row in artifacts if isinstance(row, dict) and
                row.get('path') == 'aarch64/' + name]
        if (len(rows) != 1 or type(rows[0].get('size')) is not int or
                rows[0]['size'] != (target / name).stat().st_size or
                rows[0].get('sha256') != site.get(key)):
            raise ValueError('manifest artifact hash/size mismatch')
    return site


def source_snapshot():
    result = storage_source_snapshot()
    for path in sorted((ROOT / 'apps').glob('*')):
        if path.is_file() and path.suffix in ('.S', '.ld'):
            result[path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def metadata_snapshot(boot):
    boot = Path(boot)
    return {name: hashlib.sha256((boot / name).read_bytes()).hexdigest() for name in
        ('manifest.json', 'aarch64/disk-app-compiler.json', 'aarch64/disk-app-site.json')}


def parse_disk_app_trace(output, phase, site, app):
    """Parse exact ordered guest evidence; no PASS-only or partial acceptance."""
    try:
        return _parse_disk_app_trace(output, phase, site, app)
    except (ValueError, TypeError, KeyError, IndexError, AttributeError, OverflowError):
        return None


def _parse_disk_app_trace(output, phase, site, app):
    try:
        from .disk_app_site import validate_app_bytes, verify_table_closure
    except ImportError:
        from disk_app_site import validate_app_bytes, verify_table_closure

    def need(condition):
        if not condition:
            raise ValueError('trace differs from exact disk-app contract')

    need(isinstance(output, bytes) and output.endswith(b'\n') and phase in ('run', 'reject'))
    need(site['arch'] == 'aarch64' and site['address_space'] == 'identity-image-address')
    need(site['image_start'] == 0x40080000 and site['page_size'] == PAGE)
    for digest in ('binary_sha256', 'raw_sha256'):
        need(isinstance(site[digest], str) and re.fullmatch('[0-9a-f]{64}', site[digest]))
    names = ('control', 'workspace', 'tables', 'code', 'user_stack', 'entry_stack',
             'vblk_control', 'queue', 'request', 'diagnostic', 'stack', 'vectors',
             'disk_app_vectors', 'pool', 'pool_metadata')
    need(set(site['objects']) == set(names))
    image = site['image_start'], site['image_end']
    need(all(type(value) is int and 0 <= value < 1 << 64 for value in image))
    need(image[0] < image[1] and site['image_size'] == image[1] - image[0])
    objects = {}
    for name in names:
        obj = site['objects'][name]
        bounds = obj['start'], obj['end']
        need(all(type(value) is int and 0 <= value < 1 << 64 for value in bounds))
        need(image[0] <= bounds[0] < bounds[1] <= image[1] and obj['bytes'] == bounds[1] - bounds[0])
        objects[name] = bounds
    file_bytes = bytes.fromhex(app['file_hex'])
    need(1 <= len(file_bytes) <= 1024 and len(file_bytes) == app['file_bytes'])
    need(hashlib.sha256(file_bytes).hexdigest() == app['file_sha256'])
    if phase == 'run':
        actual = validate_app_bytes(file_bytes)
        need(all(app.get(key) == value for key, value in actual.items()))
    else:
        need(app['reject_reason'] == 'MAGIC' and file_bytes[:8] != b'STYAPP1\0')
    lines = [line[:-1] if line.endswith(b'\r') else line for line in output.split(b'\n')[:-1]]
    if lines and re.fullmatch(rb'[^\r\n:]*qemu-system-aarch64: terminating on signal 15 from pid [1-9][0-9]* \([^\r\n]*\)', lines[-1]):
        lines.pop()
    cursor = 0

    def exact(line):
        nonlocal cursor
        need(cursor < len(lines) and lines[cursor] == line)
        cursor += 1

    def fields(label, keys, prefix=b'STYLEOS DISKAPP '):
        nonlocal cursor
        pattern = re.escape(prefix + label.encode())
        pattern += b''.join(b' ' + key.encode() + b'=' + HEX64 for key in keys)
        need(cursor < len(lines))
        match = re.fullmatch(pattern, lines[cursor])
        need(match is not None)
        cursor += 1
        return dict(zip(keys, (int(value, 16) for value in match.groups())))

    def inside(inner, outer):
        return outer[0] <= inner[0] < inner[1] <= outer[1]

    def overlap(a, b):
        return a[0] < b[1] and b[0] < a[1]

    def includes(rows, inner):
        return any(inside(inner, outer) for outer in rows)

    for line in (b'STYLEOS AARCH64: initial EL1', b'STYLEOS AARCH64: current EL1',
                 b'STYLEOS AARCH64: DTB header OK', b'STYLEOS kernel_main entered',
                 b'STYLEOS boot_info validated v1', b'STYLEOS HANDOFF PASS aarch64',
                 b'STYLEOS MEMMAP DIAGNOSTIC ONLY'):
        exact(line)
    ranges = {}
    for kind in ('IMAGE', 'STACK', 'HANDOFF', 'SOURCE', 'RAM', 'RESERVED', 'CANDIDATE'):
        rows = []
        pattern = rb'STYLEOS MEMMAP ' + kind.encode() + b' ' + HEX64 + b' ' + HEX64
        while cursor < len(lines):
            match = re.fullmatch(pattern, lines[cursor])
            if not match:
                break
            a, b = (int(value, 16) for value in match.groups())
            need(a < b and (not rows or rows[-1][1] < a))
            rows.append((a, b))
            cursor += 1
        need(rows and (kind not in ('IMAGE', 'STACK', 'HANDOFF', 'SOURCE') or len(rows) == 1))
        ranges[kind] = rows
    exact(b'STYLEOS MEMMAP PASS')
    need(ranges['IMAGE'] == [image] and ranges['STACK'] == [objects['stack']])
    need(ranges['RAM'] == [(0x40000000, 0x48000000)])
    source, handoff = ranges['SOURCE'][0], ranges['HANDOFF'][0]
    need(source[0] % 8 == 0 and 40 <= source[1] - source[0] <= 16 * 1024 * 1024)
    need(handoff[0] % 8 == 0 and handoff[1] - handoff[0] == 96)
    need(inside(handoff, objects['stack']) and inside(image, ranges['RAM'][0]) and
         inside(source, ranges['RAM'][0]) and not overlap(source, image))
    need(all(includes(ranges['RESERVED'], value) for value in
             (image, source, (0x9000000, 0x9001000))))
    for kind in ('RESERVED', 'CANDIDATE'):
        need(all((a | b) % PAGE == 0 for a, b in ranges[kind]))
    events = {}
    for group, kind in enumerate(('RAM', 'RESERVED', 'CANDIDATE')):
        for a, b in ranges[kind]:
            events.setdefault(a, [0, 0, 0])[group] += 1
            events.setdefault(b, [0, 0, 0])[group] -= 1
    active = [0, 0, 0]
    for address in sorted(events):
        active = [value + delta for value, delta in zip(active, events[address])]
        need(active[2] == int(active[0] == 1 and active[1] == 0))
    for index, (name, bounds) in enumerate(objects.items()):
        need(not overlap(bounds, source))
        if name != 'stack':
            need(not overlap(bounds, handoff))
        need(not any(overlap(bounds, prior) for prior in list(objects.values())[:index]))
        need(includes(ranges['RESERVED'], bounds) and
             not any(overlap(bounds, row) for row in ranges['CANDIDATE']))
    exact(b'STYLEOS DISKAPP PHASE name=' + phase.encode())
    guest_objects = [('tables', objects['tables']), ('control', objects['control']),
        ('entry_stack', objects['entry_stack']), ('code', objects['code']),
        ('user_stack', objects['user_stack']), ('workspace', objects['workspace']),
        ('queue', objects['queue']), ('request', objects['request']),
        ('vblk_control', objects['vblk_control']), ('vectors', objects['disk_app_vectors']),
        ('old_vectors', objects['vectors']), ('stack', objects['stack']),
        ('handoff', handoff), ('source', source), ('diagnostic', objects['diagnostic']),
        ('pool', objects['pool']), ('pool_metadata', objects['pool_metadata'])]
    for name, bounds in guest_objects:
        exact(('STYLEOS DISKAPP OBJECT name=%s 0x%016x 0x%016x' %
               (name, *bounds)).encode())
    machine_keys = ('root', 'mode', 'control0', 'control1', 'control2', 'control3',
                    'vectors', 'upper_root', 'active', 'dispatcher_sp', 'context_live', 'sp')
    before = fields('MACHINE phase=IO', machine_keys)
    need(before['mode'] & ((1 << 25) | (1 << 24) | (1 << 12) | 5) == 0 and
         before['control2'] == 4 and before['control3'] == 0x3c0 and
         before['vectors'] == objects['vectors'][0] and before['upper_root'] == 0 and
         before['active'] == (1 << 64) - 1 and before['context_live'] == 0 and
         before['dispatcher_sp'] == 0 and before['sp'] % 16 == 0 and
         inside((before['sp'], before['sp'] + 16), objects['stack']))
    fdt = fields('FDT', ('source', 'bytes', 'candidates'))
    need(fdt == dict(source=source[0], bytes=source[1] - source[0], candidates=32))
    device = fields('DEVICE', ('base', 'bytes', 'version', 'id', 'features',
                              'negotiated', 'sectors', 'queue'))
    required = (1 << 32) | (1 << 9)
    need(0xa000000 <= device['base'] < 0xa004000 and
         (device['base'] - 0xa000000) % 0x200 == 0 and device['bytes'] == 0x200 and
         device['version'] == 2 and device['id'] == 2 and device['sectors'] == 2048 and
         device['queue'] == 8 and device['negotiated'] == required and
         device['features'] & required == required and device['features'] & (1 << 5) == 0)
    exact(('STYLEOS DISKAPP READ generation=0x%016x name=%s length=0x%016x data=%s' %
           (1, FILE_NAME.hex(), len(file_bytes), file_bytes.hex())).encode())
    drain = fields('DRAIN', ('reads', 'writes', 'flushes', 'requests', 'avail', 'used'))
    need(drain == dict(reads=14, writes=0, flushes=0, requests=14, avail=14, used=14))
    common = dict(phase=phase, granted=0, image_start=image[0], image_end=image[1],
        objects={key: dict(start=a, end=b) for key, (a, b) in objects.items()},
        memory={key.lower(): [list(row) for row in rows] for key, rows in ranges.items()},
        io_machine=before, fdt=fdt, device=device, io=drain,
        file_sha256=app['file_sha256'], file_hex=file_bytes.hex(),
        file_payload_crc=zlib.crc32(file_bytes.ljust(1024, b'\0')) & 0xffffffff)
    if phase == 'reject':
        exact(b'STYLEOS DISKAPP REJECT reason=' + app['reject_reason'].encode())
        exact(b'STYLEOS KERNEL HALT')
        need(cursor == len(lines))
        return dict(common, rejected=True, launches=[])
    fmt = fields('FORMAT', ('file', 'code', 'entry', 'fault', 'exit', 'marker', 'exit_code', 'checksum'))
    need(fmt == dict(file=len(file_bytes), code=app['code_bytes'], entry=app['entry_offset'],
        fault=app['fault_offset'], exit=app['exit_offset'], marker=app['marker'],
        exit_code=app['exit_code'], checksum=app['checksum']))
    copy_keys = ('source', 'destination', 'bytes', 'crc', 'va')
    nonlocal_pattern = re.escape(b'STYLEOS DISKAPP COPY') + b''.join(
        b' ' + key.encode() + b'=' + HEX64 for key in copy_keys) + rb' data=([0-9a-f]+)'
    need(cursor < len(lines))
    match = re.fullmatch(nonlocal_pattern, lines[cursor])
    need(match is not None)
    cursor += 1
    copied = dict(zip(copy_keys, (int(value, 16) for value in match.groups()[:-1])))
    code = bytes.fromhex(app['code_hex'])
    code_page = code.ljust(PAGE, b'\0')
    need(match.groups()[-1].decode() == code_page.hex())
    need(copied['destination'] == objects['code'][0] and copied['bytes'] == len(code) and
         copied['crc'] == zlib.crc32(code.ljust(PAGE, b'\0')) & 0xffffffff and
         copied['va'] == CODE_VA and
         inside((copied['source'] - app['header_bytes'], copied['source'] + len(code)), objects['control']))
    # Complete finite sparse dumps reconstruct every zero as well as nonzero
    # descriptor. The proof walks all leaves and refuses foreign user/MMIO VAs.
    safe = fields('MACHINE phase=SAFE', machine_keys)
    closures = {}
    roots = {}
    for root_id, is_user in (('SAFE', False), ('APP', True)):
        root = fields('ROOT id=' + root_id, ('pa', 'pages'))
        reserve = objects['tables'][0] + (32 * PAGE if is_user else 0)
        need(root['pa'] == reserve and 0 < root['pages'] <= 32)
        page_addresses = list(range(reserve, reserve + 32 * PAGE, PAGE))
        pages = {pa: [0] * 512 for pa in page_addresses}
        count = 0
        previous = None
        pattern = re.escape(('STYLEOS DISKAPP TABLE id=' + root_id).encode())
        pattern += b''.join(b' ' + key + b'=' + HEX64 for key in (b'table', b'index', b'entry'))
        while cursor < len(lines):
            match = re.fullmatch(pattern, lines[cursor])
            if match is None:
                break
            pa, index, value = (int(value, 16) for value in match.groups())
            need(pa in pages and index < 512 and value != 0 and
                 (previous is None or previous < (pa, index)))
            pages[pa][index] = value
            previous = pa, index
            count += 1
            cursor += 1
        end = fields('TABLES END id=' + root_id, ('entries',))
        need(end['entries'] == count)
        closure = verify_table_closure(root['pa'], page_addresses, pages, site, user=is_user)
        need(closure['used_pages'] == root['pages'])
        closures[root_id] = closure
        roots[root_id] = root['pa']
    need(safe['root'] == roots['SAFE'] and safe['mode'] == 0x30d00801 and
         safe['control0'] & 0xffffffff == 0x80903010 and
         safe['control0'] >> 32 in range(6) and safe['control1'] == 0x44 and
         safe['control2'] == 4 and safe['control3'] == 0x3c0 and
         safe['vectors'] == objects['disk_app_vectors'][0] and safe['upper_root'] == 0 and
         safe['active'] == (1 << 64) - 1 and safe['context_live'] == 0 and
         safe['dispatcher_sp'] == 0 and safe['sp'] % 16 == 0 and
         inside((safe['sp'], safe['sp'] + 16), objects['stack']))
    launches = []
    for arg in (0, 1):
        run = fields('RUN', ('arg', 'entry', 'sp', 'root'))
        need(run == dict(arg=arg, entry=CODE_VA + app['entry_offset'],
                         sp=STACK_VA + PAGE, root=roots['APP']))
        event = fields('EVENT arg=0x%016x kind=%s' % (arg, 'fault' if arg == 0 else 'exit'),
            ('pc', 'sp', 'status', 'vector', 'syndrome', 'fault_va', 'root', 'kernel_sp', 'x0', 'x1', 'x19'))
        need(event['pc'] == CODE_VA + (app['fault_offset'] if arg == 0 else app['code_bytes']) and
             event['sp'] == STACK_VA + PAGE and event['status'] == 0x3c0 and event['vector'] == 8 and
             event['syndrome'] == (0x92000006 if arg == 0 else 0x56000002) and
             event['fault_va'] == (FAULT_VA if arg == 0 else 0) and event['root'] == roots['APP'] and
             event['kernel_sp'] % 16 == 0 and
             inside((event['kernel_sp'], event['kernel_sp'] + 16), objects['entry_stack']) and
             event['x0'] == (0 if arg == 0 else app['exit_code']) and
             event['x1'] == (FAULT_VA if arg == 0 else 0) and
             event['x19'] == (0 if arg == 0 else app['marker']))
        returned = fields('SAFE', ('arg', 'root', 'sp'))
        need(returned['arg'] == arg and returned['root'] == roots['SAFE'] and
             returned['sp'] % 16 == 0 and inside((returned['sp'], returned['sp'] + 16), objects['stack']))
        observed = fields('MACHINE phase=SAFE', machine_keys)
        need(all(observed[key] == safe[key] for key in machine_keys if key != 'sp') and
             observed['sp'] % 16 == 0 and
             inside((observed['sp'], observed['sp'] + 16), objects['stack']))
        exact(('STYLEOS DISKAPP RELEASE arg=0x%016x state=FREE' % arg).encode())
        launches.append(dict(run=run, event=event, safe=returned))
    exact(b'STYLEOS DISKAPP PASS')
    exact(b'STYLEOS KERNEL HALT')
    need(cursor == len(lines))
    return dict(common, rejected=False, format=fmt, copy=copied,
                code_sha256=hashlib.sha256(code).hexdigest(),
                code_page_sha256=hashlib.sha256(code_page).hexdigest(),
                code_page_hex=code_page.hex(), safe_machine=safe,
                table_closures=closures, launches=launches)



def collect_vm(command, phase, site, app, timeout=25):
    """Require a complete stable guest halt before intentionally stopping QEMU."""
    if phase not in ('run', 'reject') or timeout <= 0:
        raise ValueError('invalid disk-app phase or timeout')
    output = bytearray()
    process = None
    error = None
    seen_at = None
    settled = natural_exit = requested_stop = forced_stop = False
    trace = None
    selector = selectors.DefaultSelector()
    started = time.monotonic()
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector.register(process.stdout, selectors.EVENT_READ)
        while time.monotonic() - started < timeout:
            if len(output) >= 4 * 1024 * 1024:
                error = 'UART output exceeded bounded capture size'
                break
            for key, _ in selector.select(timeout=0.05):
                chunk = os.read(key.fileobj.fileno(), 65536)
                if chunk:
                    output.extend(chunk)
                else:
                    selector.unregister(key.fileobj)
            if process.poll() is not None:
                natural_exit = True
                error = 'QEMU exited or reset before intentional stop'
                break
            complete = output.split(b'\n')[:-1]
            if any(line.rstrip(b'\r') == b'STYLEOS KERNEL HALT' for line in complete):
                if seen_at is None:
                    seen_at = time.monotonic()
                if time.monotonic() - seen_at >= 0.2:
                    trace = parse_disk_app_trace(bytes(output), phase, site, app)
                    if trace is None:
                        error = 'terminal marker without valid complete disk-app trace'
                    else:
                        settled = True
                    break
            if any(b'STYLEOS DISKAPP ERROR' in line or b'STYLEOS EXCEPTION' in line or
                   b'STYLEOS EMERGENCY HALT' in line for line in complete):
                error = 'guest reported disk-app or kernel exception error'
                break
        else:
            error = 'timeout before valid terminal trace'
    except (OSError, ValueError) as exc:
        error = str(exc)
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
    code = process.returncode if process else None
    if error is None:
        if (natural_exit or not requested_stop or forced_stop or not settled or
                code not in (0, -signal.SIGTERM)):
            error = 'QEMU process stop did not match required lifecycle'
        else:
            trace = parse_disk_app_trace(bytes(output), phase, site, app)
            if trace is None:
                error = 'disk-app trace changed or became invalid during stop'
    return dict(status='FAIL' if error else 'PASS', detail=error or
        'validated complete guest halt, then intentional SIGTERM', command=list(map(str, command)),
        pid=process.pid if process else None, phase=phase,
        uart_sha256=hashlib.sha256(output).hexdigest(), requested_stop=requested_stop,
        forced_stop=forced_stop, natural_exit=natural_exit, settled=settled,
        stop_signal=int(signal.SIGTERM) if requested_stop else None,
        returncode=code, trace=trace if error is None else None, uart=bytes(output))


class DiskAppChecks:
    def __init__(self, run):
        self.run = Path(run)
        self.logs = self.run / 'test-results'
        self.results = []

    def record(self, name, status, detail, log=''):
        write_new(self.logs / (name + '.log'), log)
        self.results.append(dict(name=name, status=status, detail=detail, log=name + '.log'))
        print(f'{name}: {status} — {detail}', flush=True)

    def command(self, name, commands):
        log = []
        for command in commands:
            command = list(map(str, command))
            log.append('$ ' + ' '.join(command))
            try:
                result = subprocess.run(command, cwd=ROOT, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
            except (OSError, subprocess.TimeoutExpired) as exc:
                self.record(name, 'FAIL', str(exc), '\n'.join(log))
                return False
            log.append(result.stdout)
            if result.returncode:
                self.record(name, 'FAIL', 'exit ' + str(result.returncode), '\n'.join(log))
                return False
        self.record(name, 'PASS', 'exit 0', '\n'.join(log))
        return True

    def host(self):
        cc = os.environ.get('CC', 'cc')
        flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Iinclude']
        definitions = [('host-storage-format', ['tests/test_storage.c', 'kernel/storage.c'], []),
            ('host-storage-fdt', ['tests/test_storage_fdt.c', 'boot/aarch64/storage_fdt.c'], []),
            ('host-storage-transport', ['tests/test_virtio_block.c', 'arch/aarch64/virtio_block.c'],
             ['-DSTYLEOS_VBLK_HOST_TEST=1']),
            ('host-disk-app-format', ['tests/test_disk_app.c', 'kernel/disk_app.c'], []),
            ('host-disk-app-fdt', ['tests/test_disk_app_fdt.c', 'boot/aarch64/disk_app_fdt.c'], [])]
        good = True
        for name, sources, extra in definitions:
            exe = self.run / 'host-tests' / name
            good = self.command(name, [[cc, *flags, *extra, *sources, '-o', exe], [exe]]) and good
        # Focused patterns prevent recursively invoking a full runner suite.
        for name, pattern in [('host-storage-python', 'test_storage*.py'),
                              ('host-disk-app-python', 'test_disk_app*.py')]:
            good = self.command(name, [[sys.executable, '-m', 'unittest', 'discover',
                '-s', 'tests', '-p', pattern, '-v']]) and good
        return good

    def vm(self, name, disk, identity, phase, qemu, boot, site, app):
        before = disk_state(disk, identity)
        command = qemu_command(qemu, boot / 'aarch64/styleos.bin', disk, phase)
        result = collect_vm(command, phase, site, app, self.timeout)
        uart = result.pop('uart')
        after = disk_state(disk, identity)
        result.update(stage=name, disk_before=before, disk_after=after,
            binary_sha256=site['binary_sha256'], raw_sha256=site['raw_sha256'],
            manifest_sha256=hashlib.sha256((boot / 'manifest.json').read_bytes()).hexdigest(),
            app=app)
        if result['status'] == 'PASS' and before['sha256'] != after['sha256']:
            result.update(status='FAIL', detail='read-only guest changed fixture disk hash', trace=None)
        write_new(self.logs / (name + '.uart'), uart)
        json_new(self.logs / (name + '.json'), result)
        log = '$ ' + ' '.join(command) + '\n' + uart.decode('utf-8', 'replace')
        log += '\n[harness] ' + json.dumps({key: result[key] for key in
            ('pid', 'requested_stop', 'forced_stop', 'natural_exit', 'settled',
             'stop_signal', 'returncode')}) + '\n'
        self.record(name, result['status'], result['detail'], log)
        return result


def execute(run, timeout):
    checks = DiskAppChecks(run)
    checks.timeout = timeout
    provenance = dict(created_utc=datetime.now(timezone.utc).isoformat(), run=str(run),
        sources_sha256=source_snapshot(), host_platform=platform.platform(),
        python_version=sys.version,
        host_disk_mutations='new regular fixture files only, C packer before every VM; no host disk writes during or after VM',
        qemu_profile='virt/cortex-a57/one CPU/128MiB/TCG/modern VirtIO MMIO/polling')
    try:
        provenance['git_head'] = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        provenance['git_dirty'] = bool(subprocess.check_output(
            ['git', 'status', '--porcelain'], cwd=ROOT))
    except (OSError, subprocess.CalledProcessError):
        provenance['git_head'] = None
    boot = run / 'boot'
    try:
        provenance['tool_versions'] = tool_versions()
        if not checks.host():
            return checks, provenance
        if not checks.command('build-disk-app-boot', [[sys.executable, ROOT / 'tools/build.py',
                '--arch', 'aarch64', '--test-disk-app', '--output', boot]]):
            return checks, provenance
        site = verified_disk_app_site(boot)
        metadata = metadata_snapshot(boot)
        provenance.update(site=site, boot_metadata_sha256=metadata)
        checks.record('disk-app-binary-proof', 'PASS',
            'independent ELF/Image/manifest/source-profile proof', json.dumps(site, indent=2))
        apps = run / 'apps'
        apps.mkdir()
        for variant in ('a', 'b'):
            marker, result = (0x1818a001, 41) if variant == 'a' else (0x1818b002, 73)
            if not checks.command('build-app-' + variant, [[sys.executable,
                    ROOT / 'tools/build_disk_app.py', '--output', apps / variant,
                    '--marker', hex(marker), '--exit-code', str(result)]]):
                return checks, provenance
        artifacts = {variant: verified_app_artifact(apps / variant) for variant in ('a', 'b')}
        invalid = apps / 'invalid'
        invalid.mkdir()
        invalid_bytes = bytearray(bytes.fromhex(artifacts['a']['file_hex']))
        invalid_bytes[0] ^= 1
        write_new(invalid / 'app.bin', invalid_bytes)
        artifacts['invalid'] = dict(artifacts['a'], file_hex=bytes(invalid_bytes).hex(),
            file_sha256=hashlib.sha256(invalid_bytes).hexdigest(), reject_reason='MAGIC')
        json_new(invalid / 'app.json', artifacts['invalid'])
        cc = os.environ.get('CC', 'cc')
        packer = run / 'host-tests' / 'pack-disk-app'
        if not checks.command('build-fixture-packer', [[cc, '-std=c11', '-O2', '-Wall',
                '-Wextra', '-Werror', '-Iinclude', 'tools/pack_disk_app.c', 'kernel/storage.c',
                '-o', packer]]):
            return checks, provenance
        disks = {name: fixture_disk(run, name, apps / variant / 'app.bin', packer)
                 for name, variant, _phase in STAGES}
        provenance['initial_disks'] = {name: state for name, (_path, state) in disks.items()}
        provenance['app_artifacts'] = artifacts
        qemu = shutil.which(os.environ.get('QEMU_AARCH64', 'qemu-system-aarch64'))
        if not qemu:
            checks.record('qemu-disk-app', 'SKIPPED', 'qemu-system-aarch64 not found')
            return checks, provenance
        version = subprocess.run([qemu, '--version'], text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=10)
        if version.returncode:
            checks.record('qemu-disk-app', 'FAIL', 'QEMU version command failed', version.stdout)
            return checks, provenance
        provenance['qemu_version'] = version.stdout
        completed = []
        provenance['qemu_processes'] = []
        for name, variant, phase in STAGES:
            if verified_disk_app_site(boot) != site or metadata_snapshot(boot) != metadata:
                raise ValueError('boot ELF/Image/manifest changed between VMs')
            if variant in ('a', 'b') and verified_app_artifact(apps / variant) != artifacts[variant]:
                raise ValueError('external app artifact changed between VMs')
            if hashlib.sha256((apps / variant / 'app.bin').read_bytes()).hexdigest() != artifacts[variant]['file_sha256']:
                raise ValueError('application file changed after fixture packing')
            disk, identity = disks[name]
            result = checks.vm(name, disk, identity, phase, qemu, boot, site, artifacts[variant])
            completed.append(result)
            provenance['qemu_processes'].append({key: result[key] for key in
                ('stage', 'phase', 'pid', 'command')})
            if result['status'] != 'PASS':
                break
        if len(completed) == len(STAGES) and all(row['status'] == 'PASS' for row in completed):
            unchanged = verified_disk_app_site(boot) == site and metadata_snapshot(boot) == metadata
            unchanged = unchanged and source_snapshot() == provenance['sources_sha256']
            distinct = len({row['pid'] for row in completed}) == len(STAGES)
            replaced = artifacts['a']['file_sha256'] != artifacts['b']['file_sha256']
            checks.record('disk-app-replacement', 'PASS' if unchanged and distinct and replaced else 'FAIL',
                'different external artifacts, three independent VMs, byte-identical boot artifacts and unchanged source hashes')
    except (OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError) as exc:
        checks.record('disk-app-runner-error', 'FAIL', str(exc))
    return checks, provenance


def verified_app_artifact(folder):
    """Independently derive external ELF/container facts; sidecars are evidence only."""
    try:
        from .disk_app_site import build_app_metadata
    except ImportError:
        from disk_app_site import build_app_metadata
    folder = Path(folder)
    metadata = json.loads((folder / 'app.json').read_text())
    actual = build_app_metadata(folder / 'disk_demo.elf',
        source_path=ROOT / 'apps/disk_demo.S', raw_path=folder / 'disk_demo.raw')
    if (not isinstance(metadata, dict) or any(metadata.get(key) != value for key, value in actual.items()) or
            (folder / 'app.bin').read_bytes().hex() != actual['file_hex']):
        raise ValueError('external application metadata differs from ELF/container bytes')
    return actual


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/disk-app-tests')
    parser.add_argument('--timeout', type=float, default=25)
    args = parser.parse_args()
    if not 0 < args.timeout <= 300:
        parser.error('--timeout must be positive and no greater than 300 seconds')
    try:
        run = new_run_directory(args.output)
        print('DISK APP RUN DIRECTORY:', run, flush=True)
        checks, provenance = execute(run, args.timeout)
        json_new(run / 'provenance.json', provenance)
        json_new(checks.logs / 'results.json', checks.results)
        failed = any(row['status'] == 'FAIL' for row in checks.results)
        skipped = any(row['status'] == 'SKIPPED' for row in checks.results)
        code = 1 if failed else 2 if skipped else 0
        json_new(run / 'run.json', dict(run=str(run), status='FAIL' if failed else
            'SKIPPED' if skipped else 'PASS', exit_code=code,
            passed=sum(row['status'] == 'PASS' for row in checks.results),
            total=len(checks.results)))
        print(f'DISK APP RESULTS: {sum(row["status"] == "PASS" for row in checks.results)}/{len(checks.results)} PASS', flush=True)
        return code
    except (OSError, ValueError) as exc:
        print('disk app runner: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
