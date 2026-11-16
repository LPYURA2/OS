#!/usr/bin/env python3
"""Run the bounded ARM storage experiment in fresh, runner-owned files only.

The host creates zero-filled regular files. It never constructs or repairs a
StyleOS file-store record. Every payload/commit write is performed by the guest.
Each invocation gets a new directory, preserving every earlier run.
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
import stat
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
DISK_BYTES = 1024 * 1024
PAYLOAD_A = b'StyleOS note A\n'
PAYLOAD_B = b'StyleOS note B: updated\n'
FILE_NAME = b'note.txt'
PHASES = ('init-a', 'read-a', 'update-b', 'read-b', 'prepare-b', 'recover-a')
STAGES = (('persist-init-a', 'persist', 'init-a'),
          ('persist-read-a', 'persist', 'read-a'),
          ('persist-update-b', 'persist', 'update-b'),
          ('persist-read-b', 'persist', 'read-b'),
          ('crash-init-a', 'crash', 'init-a'),
          ('crash-prepare-b', 'crash', 'prepare-b'),
          ('crash-recover-a', 'crash', 'recover-a'))
OBJECTS = ('control', 'workspace', 'queue', 'request', 'vblk_control',
           'stack', 'diagnostic', 'pool', 'pool_metadata', 'vectors')
HEX64 = rb'0x([0-9a-f]{16})'


def parse_storage_trace(output, phase, site):
    """Consume one complete guest trace, binding every field to this profile.

    A PASS/barrier alone cannot establish storage success. In particular, the
    exact operation counts include the guest's full blank-disk scan, mount,
    sector transfers and every acknowledged protocol flush. No host disk-format parser
    is used: the file name and payload evidence comes from the guest itself.
    """
    try:
        return _parse_storage_trace(output, phase, site)
    except (ValueError, TypeError, KeyError, IndexError, AttributeError, OverflowError):
        return None


def _parse_storage_trace(output, phase, site):
    def need(condition):
        if not condition:
            raise ValueError('storage trace differs from the exact guest contract')

    need(isinstance(output, bytes) and output.endswith(b'\n') and phase in PHASES)
    need(site['arch'] == 'aarch64' and site['address_space'] == 'identity-image-address')
    need(site['image_start'] == 0x40080000 and site['page_size'] == 4096)
    need(set(site['objects']) == set(OBJECTS))
    for key in ('binary_sha256', 'raw_sha256'):
        need(isinstance(site[key], str) and re.fullmatch('[0-9a-f]{64}', site[key]))
    objects = {}
    image = site['image_start'], site['image_end']
    need(all(type(value) is int and 0 <= value < 1 << 64 for value in image))
    need(image[0] < image[1] and site['image_size'] == image[1] - image[0])
    for name in OBJECTS:
        obj = site['objects'][name]
        bounds = obj['start'], obj['end']
        need(all(type(value) is int and 0 <= value < 1 << 64 for value in bounds))
        need(image[0] <= bounds[0] < bounds[1] <= image[1] and
             obj['bytes'] == bounds[1] - bounds[0])
        objects[name] = bounds

    lines = output.split(b'\n')[:-1]
    lines = [line[:-1] if line.endswith(b'\r') else line for line in lines]
    # QEMU's expected SIGTERM diagnostic is host output, and may occur only
    # after the complete ordinary guest halt. Every other extra line fails.
    if lines and re.fullmatch(rb'[^\r\n:]*qemu-system-aarch64: terminating on signal 15 from pid [1-9][0-9]* \([^\r\n]*\)', lines[-1]):
        need(phase != 'prepare-b')
        lines.pop()
    cursor = 0

    def exact(line):
        nonlocal cursor
        need(cursor < len(lines) and lines[cursor] == line)
        cursor += 1

    def fields(label, names):
        nonlocal cursor
        pattern = re.escape(b'STYLEOS STORAGE ' + label.encode())
        pattern += b''.join(b' ' + name.encode() + b'=' + HEX64 for name in names)
        need(cursor < len(lines))
        match = re.fullmatch(pattern, lines[cursor])
        need(match is not None)
        cursor += 1
        return dict(zip(names, (int(value, 16) for value in match.groups())))

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

    def inside(inner, outer):
        return outer[0] <= inner[0] < inner[1] <= outer[1]

    def overlap(a, b):
        return a[0] < b[1] and b[0] < a[1]

    def includes(rows, inner):
        return any(inside(inner, outer) for outer in rows)

    need(inside(handoff, objects['stack']) and inside(image, ranges['RAM'][0]) and
         inside(source, ranges['RAM'][0]) and not overlap(source, image))
    need(all(includes(ranges['RESERVED'], value) for value in
             (image, source, (0x9000000, 0x9001000))))
    for kind in ('RESERVED', 'CANDIDATE'):
        need(all((a | b) % 4096 == 0 for a, b in ranges[kind]))
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
             not any(overlap(bounds, candidate) for candidate in ranges['CANDIDATE']))

    exact(b'STYLEOS STORAGE PHASE name=' + phase.encode())
    for name in OBJECTS:
        bounds = objects[name]
        exact(('STYLEOS STORAGE OBJECT name=%s 0x%016x 0x%016x' %
               (name, *bounds)).encode())
    machine = fields('MACHINE', ('el', 'sctlr', 'daif', 'sp', 'vectors'))
    need(machine['el'] == 4 and machine['daif'] == 0x3c0 and
         machine['sctlr'] & ((1 << 25) | (1 << 24) | (1 << 12) | 5) == 0 and
         machine['sp'] % 16 == 0 and
         inside((machine['sp'], machine['sp'] + 16), objects['stack']) and
         machine['vectors'] == objects['vectors'][0])
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
    reads = []
    progress = []

    def read(payload, generation):
        exact(('STYLEOS STORAGE READ generation=0x%016x name=%s length=0x%016x data=%s' %
               (generation, FILE_NAME.hex(), len(payload), payload.hex())).encode())
        reads.append(dict(generation=generation, name_hex=FILE_NAME.hex(),
                          length=len(payload), data_hex=payload.hex()))

    def operation(label, payload, generation, counters):
        values = fields(label, ('generation', 'length', 'payload_crc', 'reads', 'writes', 'flushes'))
        expected = dict(generation=generation, length=len(payload),
                        payload_crc=zlib.crc32(payload.ljust(1024, b'\0')),
                        **dict(zip(('reads', 'writes', 'flushes'), counters)))
        need(values == expected)
        progress.append(dict(operation=label.lower(), **values))

    if phase == 'init-a':
        exact(b'STYLEOS STORAGE INIT EMPTY')
        operation('PREPARED', PAYLOAD_A, 1, (2048, 5, 3))
        operation('COMMITTED', PAYLOAD_A, 1, (2048, 6, 4))
        read(PAYLOAD_A, 1)
        counts = (2053, 6, 4)
    else:
        read(PAYLOAD_B if phase == 'read-b' else PAYLOAD_A, 2 if phase == 'read-b' else 1)
        counts = (14, 0, 0)
        if phase in ('update-b', 'prepare-b'):
            operation('PREPARED', PAYLOAD_B, 2, (14, 4, 2))
            counts = (14, 4, 2)
            if phase == 'update-b':
                operation('COMMITTED', PAYLOAD_B, 2, (14, 5, 3))
                read(PAYLOAD_B, 2)
                counts = (19, 5, 3)
    io = fields('IO', ('reads', 'writes', 'flushes', 'requests'))
    need(io == dict(zip(('reads', 'writes', 'flushes', 'requests'), (*counts, sum(counts)))))
    if phase == 'prepare-b':
        exact(b'STYLEOS STORAGE BARRIER BEFORE_COMMIT')
    else:
        exact(b'STYLEOS STORAGE PASS')
        exact(b'STYLEOS KERNEL HALT')
    need(cursor == len(lines))
    return dict(phase=phase, granted=0, image_start=image[0], image_end=image[1],
                objects={key: dict(start=a, end=b) for key, (a, b) in objects.items()},
                memory={key.lower(): [list(row) for row in rows] for key, rows in ranges.items()},
                machine=machine, fdt=fdt, device=device, reads=reads, progress=progress,
                io=io, barrier=phase == 'prepare-b')


def write_new(path, data):
    """Exclusive evidence creation; an earlier file is never overwritten."""
    path = Path(path)
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(data.encode('utf-8') if isinstance(data, str) else data)


def json_new(path, value):
    write_new(path, json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def new_run_directory(output):
    output = Path(output).absolute()
    if output.is_symlink():
        raise ValueError('output itself must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    if not output.is_dir():
        raise ValueError('output must be a directory')
    output = output.resolve(strict=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    run = Path(tempfile.mkdtemp(prefix='run-' + stamp + '-', dir=output))
    for name in ('disks', 'test-results', 'host-tests'):
        (run / name).mkdir()
    return run


def create_zero_disk(run, name):
    """Create one exclusive regular file in this fresh run, never a host disk."""
    if name not in ('persist', 'crash'):
        raise ValueError('only the two fixed runner disk names are allowed')
    run = Path(run).resolve(strict=True)
    directory = run / 'disks'
    if directory.is_symlink() or directory.resolve(strict=True).parent != run:
        raise ValueError('disk directory escaped the fresh run')
    path = directory / (name + '.img')
    fd = os.open(path, os.O_RDWR | os.O_CREAT | os.O_EXCL |
                 getattr(os, 'O_NOFOLLOW', 0), 0o600)
    with os.fdopen(fd, 'w+b') as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise ValueError('created data disk is not an exclusive regular file')
        # truncate on a newly O_EXCL-created file produces only zero bytes;
        # no disk-format metadata or payload is written by the host.
        stream.truncate(DISK_BYTES)
        stream.flush()
        os.fsync(stream.fileno())
    state = disk_state(path)
    if state['sha256'] != hashlib.sha256(bytes(DISK_BYTES)).hexdigest():
        raise ValueError('newly created disk is not entirely zero')
    return path, state


def disk_state(path, identity=None):
    path = Path(path)
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0))
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or
                info.st_size != DISK_BYTES):
            raise ValueError('data disk must remain a single regular 1-MiB file')
        current = {'device': info.st_dev, 'inode': info.st_ino,
                   'bytes': info.st_size}
        if identity is not None and any(current[k] != identity[k] for k in current):
            raise ValueError('runner-owned data disk identity changed')
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(65536), b''):
            digest.update(chunk)
    return dict(path=str(path.resolve(strict=True)), **current, sha256=digest.hexdigest())


def storage_sources():
    return ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
            'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
            'kernel/exception.c', 'kernel/boot_pool.c', 'kernel/storage.c',
            'kernel/storage_demo.c', 'boot/aarch64/storage_fdt.c',
            'arch/aarch64/virtio_block.c', 'boot/aarch64/platform.c',
            'boot/aarch64/start.S', 'boot/aarch64/fdt_memory.c',
            'arch/aarch64/console.c']


def verified_storage_site(folder):
    try:
        from .storage_site import describe_storage
    except ImportError:
        from storage_site import describe_storage
    folder = Path(folder)
    target = folder / 'aarch64'
    manifest = json.loads((folder / 'manifest.json').read_text())
    modes = ('invalid_boot_info', 'invalid_memory_map', 'test_exception',
             'test_bootpool', 'test_paging', 'test_paging_fault',
             'test_ownership_plan', 'test_ownership_plan_invalid',
             'test_tasks', 'test_user_processes')
    if (not isinstance(manifest, dict) or manifest.get('test_storage') is not True or
            any(manifest.get(key) is not False for key in modes)):
        raise ValueError('storage manifest is not the isolated storage profile')
    flag_map, source_map = manifest.get('compiler_flags'), manifest.get('storage_sources')
    if (not isinstance(flag_map, dict) or not isinstance(source_map, dict) or
            set(flag_map) != {'aarch64'} or set(source_map) != {'aarch64'}):
        raise ValueError('storage architecture metadata is malformed')
    flags, sources = flag_map.get('aarch64'), source_map.get('aarch64')
    if (not isinstance(flags, list) or not flags or
            any(not isinstance(x, str) or not x for x in flags) or
            not isinstance(sources, list) or any(not isinstance(x, str) for x in sources) or
            sources != storage_sources()):
        raise ValueError('storage flags/source set differs from the dedicated build')
    define = '-DSTYLEOS_TEST_STORAGE=1'
    if (flags.count(define) != 1 or flags.count('--target=aarch64-none-elf') != 1 or
            flags.count('-DSTYLEOS_EXPECT_ARCH=2') != 1 or
            any((flag.startswith(('--target=', '-target')) and
                 flag != '--target=aarch64-none-elf') or
                (flag.startswith('-DSTYLEOS_EXPECT_ARCH') and
                 flag != '-DSTYLEOS_EXPECT_ARCH=2') or
                flag.startswith('-USTYLEOS_') or
                (flag.startswith('-DSTYLEOS_TEST_') and flag != define)
                for flag in flags)):
        raise ValueError('storage target/test compiler flags mismatch')
    compiler = json.loads((target / 'storage-compiler.json').read_text())
    if compiler != dict(flags=flags, sources=sources):
        raise ValueError('storage compiler sidecar differs from manifest')
    verified = describe_storage('aarch64', target / 'kernel.elf',
        raw_path=target / 'styleos.bin', compiler_flags=flags)
    if (verified.get('compiler_flags') != flags or
            json.loads((target / 'storage-site.json').read_text()) != verified):
        raise ValueError('storage site differs from independently verified image')
    artifacts = manifest.get('artifacts')
    if (not isinstance(artifacts, list) or len(artifacts) != 2 or
            any(not isinstance(row, dict) for row in artifacts)):
        raise ValueError('storage manifest needs exactly two ARM artifacts')
    for name, key in [('kernel.elf', 'binary_sha256'), ('styleos.bin', 'raw_sha256')]:
        rows = [row for row in artifacts if row.get('path') == 'aarch64/' + name]
        if (len(rows) != 1 or type(rows[0].get('size')) is not int or
                rows[0]['size'] != (target / name).stat().st_size or
                rows[0].get('sha256') != verified.get(key)):
            raise ValueError('storage manifest artifact hash/size mismatch')
    return verified


def qemu_command(qemu, image, disk, phase):
    if phase not in PHASES:
        raise ValueError('unknown fixed storage phase')
    # JSON blockdev avoids interpreting commas or other filename characters as
    # additional QEMU options. No caller-provided drive/device option is accepted.
    file_node = dict(driver='file', filename=str(Path(disk).resolve(strict=True)),
                     **{'node-name': 'storage-file',
                        'cache': {'direct': False, 'no-flush': False}})
    raw_node = {'driver': 'raw', 'file': 'storage-file', 'node-name': 'storage',
                'read-only': False}
    return [str(qemu), '-machine', 'virt', '-cpu', 'cortex-a57', '-smp', '1',
            '-m', '128M', '-accel', 'tcg,thread=single', '-display', 'none',
            '-serial', 'stdio', '-monitor', 'none', '-net', 'none', '-no-reboot',
            '-global', 'virtio-mmio.force-legacy=false',
            '-kernel', str(Path(image).resolve(strict=True)),
            '-append', 'styleos.storage=' + phase,
            '-blockdev', json.dumps(file_node, separators=(',', ':')),
            '-blockdev', json.dumps(raw_node, separators=(',', ':')),
            '-device', 'virtio-blk-device,drive=storage,id=storage0']


def collect_vm(command, phase, site, timeout=25):
    """Observe an actual terminal/barrier trace before intentional process stop."""
    if phase not in PHASES or timeout <= 0:
        raise ValueError('invalid phase or timeout')
    barrier = phase == 'prepare-b'
    terminal = b'STYLEOS STORAGE BARRIER BEFORE_COMMIT' if barrier else b'STYLEOS KERNEL HALT'
    output = bytearray()
    process = None
    error = None
    seen_at = None
    settled = natural_exit = requested_stop = forced_stop = False
    barrier_validated = False
    trace = None
    selector = selectors.DefaultSelector()
    started = time.monotonic()
    stop_signal = None
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector.register(process.stdout, selectors.EVENT_READ)
        while time.monotonic() - started < timeout:
            if len(output) >= 4 * 1024 * 1024:
                error = 'UART output exceeded the bounded capture size'
                break
            for key, _ in selector.select(timeout=0.05):
                chunk = os.read(key.fileobj.fileno(), 65536)
                if chunk:
                    output.extend(chunk)
                else:
                    selector.unregister(key.fileobj)
            if process.poll() is not None:
                natural_exit = True
                error = 'QEMU exited or reset before the intentional stop'
                break
            complete = output.split(b'\n')[:-1]
            if any(line.rstrip(b'\r') == terminal for line in complete):
                if seen_at is None:
                    seen_at = time.monotonic()
                if time.monotonic() - seen_at >= 0.2:
                    trace = parse_storage_trace(bytes(output), phase, site)
                    if trace is None:
                        error = 'terminal marker without a valid complete storage trace'
                    else:
                        settled = True
                        barrier_validated = barrier
                    break
            if any(b'STYLEOS STORAGE ERROR' in line or b'STYLEOS EXCEPTION' in line or
                   b'STYLEOS EMERGENCY HALT' in line for line in complete):
                error = 'guest reported a storage or exception error'
                break
        else:
            error = 'timeout before a valid terminal/barrier trace'
    except (OSError, ValueError) as exc:
        error = str(exc)
    finally:
        selector.close()
        if process is not None:
            if process.poll() is None:
                requested_stop = True
                if barrier_validated and settled and error is None:
                    stop_signal = signal.SIGKILL
                    process.kill()
                else:
                    stop_signal = signal.SIGTERM
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
                (barrier and (not barrier_validated or code != -signal.SIGKILL)) or
                (not barrier and code not in (0, -signal.SIGTERM))):
            error = 'QEMU process stop did not match the required lifecycle'
        else:
            trace = parse_storage_trace(bytes(output), phase, site)
            if trace is None:
                error = 'storage trace changed or became invalid during stop'
    return {'status': 'FAIL' if error else 'PASS', 'detail': error or
            ('validated pre-commit barrier, then intentional SIGKILL' if barrier else
             'validated guest halt, then intentional SIGTERM'),
            'command': list(map(str, command)), 'pid': process.pid if process else None,
            'phase': phase, 'uart_sha256': hashlib.sha256(output).hexdigest(),
            'requested_stop': requested_stop, 'forced_stop': forced_stop,
            'natural_exit': natural_exit, 'settled': settled,
            'barrier_validated': barrier_validated,
            'stop_signal': int(stop_signal) if stop_signal is not None else None,
            'returncode': code, 'trace': trace if error is None else None,
            'uart': bytes(output)}


class StorageChecks:
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
                       ('host-storage-fdt', ['tests/test_storage_fdt.c',
                         'boot/aarch64/storage_fdt.c'], []),
                       ('host-storage-transport', ['tests/test_virtio_block.c',
                         'arch/aarch64/virtio_block.c'], ['-DSTYLEOS_VBLK_HOST_TEST=1'])]
        good = True
        for name, sources, extra in definitions:
            exe = self.run / 'host-tests' / name
            good = self.command(name, [[cc, *flags, *extra, *sources, '-o', exe], [exe]]) and good
        good = self.command('host-storage-python', [[sys.executable, '-m', 'unittest',
            'discover', '-s', 'tests', '-p', 'test_storage*.py', '-v']]) and good
        return good

    def vm(self, name, disk, initial, phase, qemu, boot, site):
        before = disk_state(disk, initial)
        command = qemu_command(qemu, boot / 'aarch64/styleos.bin', disk, phase)
        result = collect_vm(command, phase, site, self.timeout)
        uart = result.pop('uart')
        after = disk_state(disk, initial)
        result.update(stage=name, disk_before=before, disk_after=after,
                      binary_sha256=site['binary_sha256'], raw_sha256=site['raw_sha256'],
                      manifest_sha256=hashlib.sha256((boot / 'manifest.json').read_bytes()).hexdigest())
        if result['status'] == 'PASS':
            readonly = phase in ('read-a', 'read-b', 'recover-a')
            if readonly and before['sha256'] != after['sha256']:
                result.update(status='FAIL', detail='read-only phase modified the data image')
            elif not readonly and before['sha256'] == after['sha256']:
                result.update(status='FAIL', detail='write phase did not change the data image')
        write_new(self.logs / (name + '.uart'), uart)
        json_new(self.logs / (name + '.json'), result)
        log = '$ ' + ' '.join(command) + '\n' + uart.decode('utf-8', 'replace')
        log += '\n[harness] ' + json.dumps({k: result[k] for k in
            ('pid','requested_stop','forced_stop','natural_exit','settled',
             'barrier_validated','stop_signal','returncode')}) + '\n'
        self.record(name, result['status'], result['detail'], log)
        return result


def source_snapshot():
    paths = []
    for directory in ('arch', 'boot', 'kernel', 'include', 'tools', 'tests'):
        paths.extend(p for p in (ROOT / directory).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.S', '.py', '.ld'))
    paths.append(ROOT / 'Makefile')
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(paths)}


def metadata_snapshot(boot):
    """Bind the exact manifest and both compiler/proof sidecars as evidence."""
    boot = Path(boot)
    names = ('manifest.json', 'aarch64/storage-compiler.json',
             'aarch64/storage-site.json')
    return {name: hashlib.sha256((boot / name).read_bytes()).hexdigest()
            for name in names}


def tool_versions():
    """Record the actual selected portable tools, including a Termux run."""
    versions = {}
    for label, variable, defaults in (
            ('host_cc', 'CC', ('cc',)),
            ('clang', 'CLANG', ('clang', 'clang-19', 'clang-18')),
            ('ld_lld', 'LD_LLD', ('ld.lld', 'ld.lld-19', 'ld.lld-18'))):
        selected = (os.environ[variable],) if os.environ.get(variable) else defaults
        executable = next((found for name in selected if (found := shutil.which(name))), None)
        if executable is None:
            versions[label] = dict(status='MISSING', requested=list(selected))
            continue
        command = [executable, '--version']
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, timeout=10)
            versions[label] = dict(command=command, returncode=result.returncode,
                                   output=result.stdout)
        except (OSError, subprocess.TimeoutExpired) as exc:
            versions[label] = dict(command=command, error=str(exc))
    return versions


def execute(run, timeout):
    checks = StorageChecks(run)
    checks.timeout = timeout
    provenance = {'created_utc': datetime.now(timezone.utc).isoformat(),
                  'run': str(run), 'sources_sha256': source_snapshot(),
                  'host_platform': platform.platform(), 'python_version': sys.version,
                  'host_disk_mutations': 'exclusive zero-file creation only; guest performs every format/payload/commit write',
                  'qemu_profile': 'virt/cortex-a57/one CPU/128MiB/TCG/modern VirtIO MMIO/polling'}
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
        if not checks.command('build-storage', [[sys.executable, ROOT / 'tools/build.py',
                '--arch', 'aarch64', '--test-storage', '--output', boot]]):
            return checks, provenance
        try:
            site = verified_storage_site(boot)
            metadata = metadata_snapshot(boot)
            provenance['boot_metadata_sha256'] = metadata
            checks.record('storage-binary-proof', 'PASS', 'independent ELF/Image/manifest/source-profile proof',
                          json.dumps(site, indent=2))
        except (OSError, ValueError, TypeError) as exc:
            checks.record('storage-binary-proof', 'FAIL', str(exc))
            return checks, provenance
        qemu = shutil.which(os.environ.get('QEMU_AARCH64', 'qemu-system-aarch64'))
        if not qemu:
            checks.record('qemu-storage', 'SKIPPED', 'qemu-system-aarch64 not found')
            return checks, provenance
        version = subprocess.run([qemu, '--version'], text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=10)
        if version.returncode:
            checks.record('qemu-storage', 'FAIL', 'QEMU version command failed', version.stdout)
            return checks, provenance
        provenance['qemu_version'] = version.stdout
        provenance['site'] = site
        disks = {name: create_zero_disk(run, name) for name in ('persist', 'crash')}
        provenance['initial_disks'] = {name: state for name, (_path, state) in disks.items()}
        completed = []
        provenance['qemu_processes'] = []
        for name, ident, phase in STAGES:
            disk, identity = disks[ident]
            if verified_storage_site(boot) != site or metadata_snapshot(boot) != metadata:
                raise ValueError('boot image/manifest changed between virtual machines')
            result = checks.vm(name, disk, identity, phase, qemu, boot, site)
            completed.append(result)
            provenance['qemu_processes'].append(dict(stage=result['stage'],
                phase=result['phase'], pid=result['pid'], command=result['command']))
            if result['status'] != 'PASS':
                break
        if len(completed) == len(STAGES) and all(row['status'] == 'PASS' for row in completed):
            if (verified_storage_site(boot) != site or metadata_snapshot(boot) != metadata or
                    source_snapshot() != provenance['sources_sha256']):
                checks.record('storage-source-binding', 'FAIL', 'source files changed during this run')
            else:
                checks.record('storage-source-binding', 'PASS', 'source hashes unchanged across build and seven VM processes')
            for name, selected in [('persistence-and-update', completed[:4]),
                                   ('interrupted-update-recovery', completed[4:])]:
                same = all(row['disk_before']['path'] == selected[0]['disk_before']['path'] and
                           row['disk_before']['inode'] == selected[0]['disk_before']['inode']
                           for row in selected)
                chained = all(a['disk_after'] == b['disk_before'] for a,b in zip(selected,selected[1:]))
                separate = len({row['pid'] for row in completed}) == len(STAGES)
                checks.record(name, 'PASS' if same and chained and separate else 'FAIL',
                              'same runner-created disk, independent VM processes, unchanged host file between phases'
                              if same and chained and separate else 'disk identity/hash chain or distinct-process mismatch')
    except (OSError, ValueError, TypeError, subprocess.SubprocessError) as exc:
        checks.record('storage-runner-error', 'FAIL', str(exc))
    return checks, provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/storage-tests')
    parser.add_argument('--timeout', type=float, default=25,
                        help='bounded timeout for each independent QEMU process')
    args = parser.parse_args()
    if not 0 < args.timeout <= 300:
        parser.error('--timeout must be positive and no greater than 300 seconds')
    try:
        run = new_run_directory(args.output)
        print('STORAGE RUN DIRECTORY:', run, flush=True)
        checks, provenance = execute(run, args.timeout)
        json_new(run / 'provenance.json', provenance)
        json_new(checks.logs / 'results.json', checks.results)
        failed = any(row['status'] == 'FAIL' for row in checks.results)
        skipped = any(row['status'] == 'SKIPPED' for row in checks.results)
        code = 1 if failed else 2 if skipped else 0
        json_new(run / 'run.json', dict(run=str(run), status='FAIL' if failed else
            'SKIPPED' if skipped else 'PASS', exit_code=code,
            checks=len(checks.results), results='test-results/results.json'))
        print('RESULTS:', checks.logs / 'results.json', flush=True)
        return code
    except (OSError, ValueError) as exc:
        print('STORAGE RUNNER FAIL:', exc, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
