#!/usr/bin/env python3
"""Host, reproducible-build and bounded real-QEMU checks for T-P1.1."""
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
    errors = (b'BOOT ERROR', b'BOOT_ERROR', b'early exception')
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


def serial_memory_ok(output, arch, mode, ram_mib=128, el2=False):
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
            b'MEMMAP ERROR' in output or lines.count(halt) != 1 or
            not lines.index(handoff) < lines.index(diagnostic) <
            lines.index(passed) < lines.index(halt)):
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
            cmd.append('--invalid-boot-info' if mode == 'invalid' else '--invalid-memory-map')
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
                   'invalid-map': self.output / 'negative-memory'}
        names = ('build-for-qemu', 'build-negative', 'build-negative-memory')
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
                    'normal': b'STYLEOS KERNEL HALT'}[mode]
        output = bytearray()
        started = time.monotonic()
        process = None
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
                if any(line.startswith(expected) for line in output.split(b'\n')[:-1]) or process.poll() is not None:
                    break
        except OSError as error:
            return name, 'FAIL', str(error), '$ ' + ' '.join(cmd)
        finally:
            selector.close()
            if process is not None:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                if process.stdout:
                    output.extend(process.stdout.read())
                    process.stdout.close()
        text = '$ ' + ' '.join(cmd) + '\n' + output.decode('utf-8', 'replace')
        if expected not in output:
            return name, 'FAIL', 'expected serial marker missing / timeout', text
        good = serial_memory_ok(output, arch, mode, mib, el2)
        return name, ('PASS' if good else 'FAIL'), 'UART handoff and memory contract checked; QEMU stopped by harness', text


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
