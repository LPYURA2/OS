#!/usr/bin/env python3
"""Host, reproducible-build and bounded real-QEMU checks for T-P1.2."""
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

ROOT = Path(__file__).resolve().parents[1]


def serial_handoff_ok(output, arch, invalid=False, el2=False):
    """A successful marker cannot erase a prior boot failure in the same run."""
    errors = (b'BOOT ERROR', b'BOOT_ERROR', b'early exception',
              b'STYLEOS EXCEPTION', b'STYLEOS EMERGENCY')
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
            ('host-memory', ['tests/test_boot_memory.c', 'kernel/boot_memory.c',
                             'boot/x86_64/memory_map.c', 'boot/aarch64/fdt_memory.c'])
        ]:
            exe = folder / name
            self.command(name, [[cc, *flags, *sources, '-o', exe], [exe]])
        self.command('host-python', [[sys.executable, '-m', 'unittest', 'discover',
                                    '-s', 'tests', '-p', 'test_*.py', '-v']])

    def build(self, folder, mode='normal', name='build', arch='all'):
        cmd = [sys.executable, ROOT / 'tools/build.py', '--arch', arch,
               '--output', folder]
        if mode != 'normal':
            cmd.append({'invalid': '--invalid-boot-info',
                        'invalid-map': '--invalid-memory-map',
                        'exception': '--test-exception'}[mode])
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
                   'exception': self.output / 'exception'}
        names = ('build-for-qemu', 'build-negative', 'build-negative-memory', 'build-exception')
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
        else:
            good = serial_memory_ok(output, arch, mode, mib, el2)
        if arch == 'x86_64':
            good = good and serial_idt_ready(output)
        return name, ('PASS' if good else 'FAIL'), 'UART handoff/memory/exception contract checked; QEMU stopped by harness', text


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
