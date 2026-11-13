#!/usr/bin/env python3
"""Host, reproducible-build and bounded real-QEMU checks for T-P1.0."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
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
    if invalid:
        return (b'STYLEOS BOOT_INFO ERROR: magic' in output and
                b'STYLEOS HANDOFF PASS' not in output)
    if (b'STYLEOS KERNEL HALT' not in output or
            ('STYLEOS HANDOFF PASS ' + arch).encode() not in output or
            b'BOOT_INFO ERROR' in output):
        return False
    if arch == 'aarch64':
        return (b'initial EL2' if el2 else b'initial EL1') in output
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
            ('host-uefi', ['tests/test_uefi.c', 'boot/x86_64/exit_boot.c'])
        ]:
            exe = folder / name
            self.command(name, [[cc, *flags, *sources, '-o', exe], [exe]])
        self.command('host-python', [[sys.executable, '-m', 'unittest', 'discover',
                                    '-s', 'tests', '-p', 'test_*.py', '-v']])

    def build(self, folder, invalid=False, name='build'):
        cmd = [sys.executable, ROOT / 'tools/build.py', '--arch', 'all',
               '--output', folder]
        if invalid:
            cmd.append('--invalid-boot-info')
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

    def qemu(self, timeout):
        if not self.build(self.output, name='build-for-qemu'):
            return
        negative = self.output / 'negative'
        if not self.build(negative, invalid=True, name='build-negative'):
            return
        jobs = []
        for arch, variable, default in [
            ('x86_64', 'QEMU_X86', 'qemu-system-x86_64'),
            ('aarch64', 'QEMU_AARCH64', 'qemu-system-aarch64')
        ]:
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
            for invalid in (False, True):
                name = f'qemu-{arch}-' + ('invalid' if invalid else 'normal')
                jobs.append((name, arch, binary, negative if invalid else self.output,
                             firmware, invalid, False, timeout))
            if arch == 'aarch64':
                jobs.append(('qemu-aarch64-el2', arch, binary, self.output,
                             None, False, True, timeout))
        with ThreadPoolExecutor(max_workers=3) as pool:
            for name, status, detail, text in pool.map(self.run_qemu, jobs):
                self.record(name, status, detail, text)

    def run_qemu(self, job):
        name, arch, binary, folder, firmware, invalid, el2, timeout = job
        scratch = self.logs / name
        scratch.mkdir(exist_ok=True)
        cmd = [binary, '-accel', 'tcg,thread=single', '-m', '128M', '-display', 'none',
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
        expected = b'STYLEOS BOOT_INFO ERROR: magic' if invalid else b'STYLEOS KERNEL HALT'
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
                if expected in output or process.poll() is not None:
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
        good = serial_handoff_ok(output, arch, invalid, el2)
        return name, ('PASS' if good else 'FAIL'), 'serial handoff checked; QEMU terminated by harness', text


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
    parser.add_argument('--output', type=Path, default=ROOT / 'build')
    parser.add_argument('--timeout', type=float, default=25)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    checks = Checks(args.output.resolve())
    all_checks = args.all or not (args.host or args.repro or args.qemu)
    if args.host or all_checks:
        checks.host()
    if args.repro or all_checks:
        checks.repro()
    if args.qemu or all_checks:
        checks.qemu(args.timeout)
    (checks.logs / 'results.json').write_text(json.dumps(checks.results, indent=2) + '\n')
    failed = any(r['status'] == 'FAIL' for r in checks.results)
    skipped = any(r['status'] == 'SKIPPED' for r in checks.results)
    # Missing required emulators is visible to CI, not a successful green run.
    return 1 if failed else (2 if skipped else 0)


if __name__ == '__main__':
    sys.exit(main())
