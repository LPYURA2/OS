#!/usr/bin/env python3
"""Compile, boot and verify pixels from the isolated desktop display check."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import selectors
import subprocess
import sys
import time

from test import ROOT, firmware_path


def validate_ppm(path):
    header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\n', path.read_bytes())
    if not header:
        raise ValueError('invalid QMP screendump PPM header')
    width, height = map(int, header.groups())
    pixels = header.string[header.end():]
    if not (320 <= width <= 4096 and 240 <= height <= 4096 and
            len(pixels) == 3 * width * height):
        raise ValueError('invalid screendump dimensions or size')
    def pixel(x, y):
        offset = (y * width + x) * 3
        return pixels[offset:offset+3].hex()
    unit = 5 if width >= 640 else 3
    mark = ((width - 41 * unit) // 2, height // 2 - 12 * unit)
    for x, y, expected in ((0, 0, '171c24'), (width-1, height-1, '202731'),
                           (*mark, 'e9ab58'), (0, height-37, '343d48')):
        if pixel(x, y) != expected:
            raise ValueError(f'wrong guest pixel at {x},{y}, expected {expected}, got {pixel(x,y)}')
    return {'width': width, 'height': height, 'sha256': hashlib.sha256(header.string).hexdigest()}


def boot(folder, image, qemu, firmware, has_display):
    code, vars_source = firmware
    folder.mkdir()
    nvram, uart = folder / 'vars.fd', folder / 'COM1.log'
    shutil.copyfile(vars_source, nvram)
    qemu_data = os.environ.get('QEMU_DATA_DIR')
    with (folder / 'qemu.log').open('wb') as stderr:
        cmd = [qemu, '-machine', 'q35', '-accel', 'tcg,thread=single', '-m', '128M',
               '-display', 'none', '-vga', 'std' if has_display else 'none',
               '-serial', f'file:{uart}', '-monitor', 'none', '-net', 'none', '-no-reboot',
               '-qmp', 'stdio',
               '-drive', f'if=pflash,format=raw,readonly=on,file={code}',
               '-drive', f'if=pflash,format=raw,file={nvram}',
               '-blockdev', json.dumps(dict(driver='file', filename=str(image),
                   **{'node-name': 'diskfile', 'read-only': True})),
               '-blockdev', 'driver=raw,file=diskfile,node-name=styleos,read-only=on',
               '-device', 'virtio-blk-pci,drive=styleos']
        if qemu_data:
            cmd += ['-L', qemu_data]
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline and proc.poll() is None:
                log = uart.read_bytes() if uart.is_file() else b''
                if b'STYLEOS KERNEL HALT' in log:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError(f'guest did not halt, see {folder}')
            marker = (b'STYLEOS DISPLAY PASS' if has_display else
                      b'STYLEOS DISPLAY ERROR: unsupported framebuffer or memory map')
            if ((b'STYLEOS DISPLAY PASS' in log) != has_display or
                    any(log.count(line) != 1 for line in (
                        b'STYLEOS HANDOFF PASS x86_64', b'STYLEOS MEMMAP PASS', marker)) or
                    not log.index(b'STYLEOS MEMMAP PASS') < log.index(marker) <
                        log.index(b'STYLEOS KERNEL HALT')):
                raise RuntimeError(f'unexpected graphics/boot diagnostics, see {uart}')
            if not has_display:
                return {'result': 'PASS', 'check': 'unsupported display refused'}
            shot = folder / 'display.ppm'
            with selectors.DefaultSelector() as ready:
                ready.register(proc.stdout, selectors.EVENT_READ)
                pending = bytearray()
                def message():
                    until = time.monotonic() + 5
                    while b'\n' not in pending:
                        left = until - time.monotonic()
                        if left <= 0 or not ready.select(left):
                            raise RuntimeError('timed out waiting for QMP')
                        chunk = os.read(proc.stdout.fileno(), 4096)
                        if not chunk:
                            raise RuntimeError('QMP stopped unexpectedly')
                        pending.extend(chunk)
                    end = pending.index(b'\n') + 1
                    response = json.loads(pending[:end])
                    del pending[:end]
                    return response
                if 'QMP' not in message():
                    raise RuntimeError('invalid QMP greeting')
                for command in ({'execute': 'qmp_capabilities'},
                                {'execute': 'screendump', 'arguments': {'filename': str(shot)}}):
                    proc.stdin.write(json.dumps(command).encode() + b'\n')
                    proc.stdin.flush()
                    while True:
                        response = message()
                        if 'error' in response:
                            raise RuntimeError(response['error'])
                        if 'return' in response:
                            break
            return {'result': 'PASS', **validate_ppm(shot)}
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True,
                        help='a new directory for test results')
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-Wall', '-Wextra',
                    '-Werror', '-Iinclude', 'tests/test_desktop_display.c',
                    'kernel/desktop_display.c', '-o', folder/'host-check'],
                   cwd=ROOT, check=True)
    subprocess.run([folder/'host-check'], check=True)
    print('desktop-display-host: PASS', flush=True)
    subprocess.run([sys.executable, ROOT/'tools/build.py', '--arch', 'x86_64',
                    '--test-desktop-display', '--output', folder/'image'], check=True)
    qemu = shutil.which(os.environ.get('QEMU_X86','qemu-system-x86_64'))
    firmware = (firmware_path('OVMF_CODE','OVMF_CODE'), firmware_path('OVMF_VARS','OVMF_VARS'))
    if not qemu or not all(firmware):
        print('desktop-display-qemu: SKIPPED — QEMU x86-64 or OVMF is missing')
        return 2
    image = folder/'image/x86_64/styleos.img'
    res = {case: boot(folder/case, image, qemu, firmware, case == 'display')
           for case in ('display','no-display')}
    (folder/'results.json').write_text(json.dumps(res, indent=2) + '\n')
    print('desktop-display-qemu: 2/2 PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
