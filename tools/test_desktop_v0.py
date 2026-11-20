#!/usr/bin/env python3
"""Run the interactive x86-64 desktop against real QEMU/PS-2/IDE devices."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import struct
import subprocess
import sys
import time

from make_desktop_data import provision
from test import ROOT, firmware_path

EXPECTED = 'StyleOS\nПривет'.encode('utf-8')


def digest(path):
    with path.open('rb') as src:
        return hashlib.file_digest(src, 'sha256').hexdigest()


def command_exists(name):
    found = shutil.which(name)
    if not found:
        raise RuntimeError(f'Not installed: {name}')
    return found


def image_kind(path):
    suffix = path.suffix.lower()
    return {'.iso': 'iso', '.vdi': 'vdi'}.get(suffix, 'raw')


class Frame:
    def __init__(self, path):
        raw = path.read_bytes()
        header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\n', raw)
        if not header:
            raise RuntimeError('not a VGA frame')
        self.w, self.h = map(int, header.groups())
        self.pixels = raw[header.end():]
        if len(self.pixels) != 3 * self.w * self.h:
            raise RuntimeError('truncated VGA frame')

    def check(self, x, y, color):
        pos = 3 * (self.w * y + x)
        actual = self.pixels[pos:pos + 3].hex()
        if actual != color:
            raise RuntimeError(f'guest pixel {(x, y)} is {actual}, not {color}')

    def contains_color(self, x, y, w, h, color):
        for row in range(y, y + h):
            start = 3 * (self.w * row + x)
            for i in range(start, start + 3 * w, 3):
                if self.pixels[i:i+3] == color:
                    return True
        return False

    def expect_changes_in(self, other, rectangles):
        if (self.w, self.h) != (other.w, other.h) or self.pixels == other.pixels:
            raise RuntimeError('expected actual local changes in the same display mode')
        for y in range(self.h):
            base = y * self.w * 3
            same = [(max(0, x), min(self.w, x + w)) for x, top, w, h in rectangles
                    if top <= y < top + h]
            left = 0
            for a, b in sorted(same) + [(self.w, self.w)]:
                s, e = base + left * 3, base + a * 3
                if e > s and self.pixels[s:e] != other.pixels[s:e]:
                    raise RuntimeError(f'unexpected change outside damage on scanline {y}')
                left = max(left, b)


class Desktop:
    def __init__(self, proc, uart, out):
        self.proc, self.uart, self.out = proc, uart, out
        self.buffer = bytearray()
        self.ready = selectors.DefaultSelector()
        self.ready.register(proc.stdout, selectors.EVENT_READ)

    def receive(self):
        deadline = time.monotonic() + 7
        while b'\n' not in self.buffer:
            if not self.ready.select(max(0, deadline - time.monotonic())):
                raise RuntimeError('QMP response timeout')
            chunk = os.read(self.proc.stdout.fileno(), 8192)
            if not chunk:
                raise RuntimeError('QMP closed')
            self.buffer.extend(chunk)
        n = self.buffer.index(b'\n') + 1
        result = json.loads(self.buffer[:n])
        del self.buffer[:n]
        return result

    def call(self, execute, **arguments):
        self.proc.stdin.write(json.dumps({'execute': execute, 'arguments': arguments}).encode() + b'\n')
        self.proc.stdin.flush()
        while True:
            r = self.receive()
            if 'error' in r:
                raise RuntimeError(r['error'])
            if 'return' in r:
                return r['return']

    def wait_uart(self, marker, count=1, timeout=16):
        marker = ('STYLEOS DESKTOP ' + marker).encode()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.uart.is_file() and self.uart.read_bytes().splitlines().count(marker) >= count:
                return
            if self.proc.poll() is not None:
                break
            time.sleep(.04)
        raise RuntimeError(f'{marker.decode()} not seen, inspect {self.uart}')

    def screen(self, name):
        target = self.out / (name + '.ppm')
        self.call('screendump', filename=str(target))
        return Frame(target)

    def key(self, name):
        self.call('human-monitor-command', **{'command-line': f'sendkey {name} 40'})
        time.sleep(.085)

    def move(self, x, y):
        while (self.x, self.y) != (x, y):
            dx = max(-75, min(75, x - self.x))
            dy = max(-75, min(75, y - self.y))
            self.call('input-send-event', events=[{'type': 'rel', 'data': {'axis': axis, 'value': n}}
                                                  for axis, n in (('x', dx), ('y', dy)) if n])
            self.x += dx
            self.y += dy
            time.sleep(.045)

    def button(self, down):
        self.call('input-send-event', events=[{'type': 'btn', 'data': {'button': 'left', 'down': down}}])
        time.sleep(.06)

    def click(self, x, y):
        self.move(x, y)
        self.button(True)
        self.button(False)


@contextlib.contextmanager
def vm(out, image, data, qemu, firmware, *, image_format, data_format):
    out.mkdir()
    nvram, uart = out / 'vars.fd', out / 'COM1.log'
    shutil.copyfile(firmware[1], nvram)
    cmd = [qemu, '-M', 'pc', '-accel', 'tcg,thread=single', '-m', '256', '-smp', '1',
           '-rtc', 'base=2026-11-18T12:00:00,clock=vm',
           '-net', 'none', '-display', 'none', '-vga', 'std', '-monitor', 'none', '-qmp', 'stdio',
           '-serial', f'file:{uart}', '-drive', f'if=pflash,format=raw,readonly=on,file={firmware[0]}',
           '-drive', f'if=pflash,format=raw,file={nvram}']
    if image_format == 'iso':
        cmd += ['-boot', 'order=d,strict=on',
                '-drive', f'file={image},format=raw,if=ide,index=0,media=cdrom,readonly=on']
    else:
        cmd += ['-drive', f'file={image},format={image_format},if=ide,index=0,snapshot=on']
    if data is not None:
        cmd += ['-drive', f'file={data},format={data_format},if=ide,index=2,cache=writeback']
    if os.environ.get('QEMU_DATA_DIR'):
        cmd += ['-L', os.environ['QEMU_DATA_DIR']]
    with (out / 'QEMU.log').open('wb') as error:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=error)
        desktop = Desktop(proc, uart, out)
        try:
            if 'QMP' not in desktop.receive():
                raise RuntimeError('invalid QMP greeting')
            desktop.call('qmp_capabilities')
            desktop.wait_uart('READY')
            desktop.wait_uart('KEYBOARD PASS')
            desktop.wait_uart('MOUSE PASS')
            frame = desktop.screen('initial')
            desktop.w, desktop.h = frame.w, frame.h
            desktop.x, desktop.y = frame.w // 2, frame.h * 3 // 4
            yield desktop, frame
        finally:
            desktop.ready.close()
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            proc.stdin.close()
            proc.stdout.close()


def verify_disk(path, expected=EXPECTED, *, bank=0, version=1):
    raw = path.read_bytes()
    # Slot 1, project folder. The host never writes it.
    pos = (1 + (2 + bank) * 9) * 512
    magic, generation, length, folder, slot, checksum = struct.unpack_from('<8sIIBB2xI', raw, pos)
    payload = raw[pos + 512:pos + 512 + length]
    h = 2166136261
    for byte in raw[pos:pos + 20] + payload:
        h = ((h ^ byte) * 16777619) & 0xffffffff
    if (magic, generation, length, folder, slot, checksum, payload) != (
            b'STYLED01', version, len(expected), 2, 1, h, expected):
        raise RuntimeError(f'wrong persistent file, found {payload!r}')


def project_note(q, frame):
    q.click(65, 267)
    bx, by = frame.w * 688 // 1536, (frame.h - 54) * 128 // 804
    q.click(bx + 115, by + 111)  # First document, after «Назад».
    q.wait_uart('DOCUMENT OPEN')


def raw_data(path, data_format, target):
    if data_format != 'vdi':
        return path
    subprocess.run([command_exists(os.environ.get('QEMU_IMG', 'qemu-img')),
                    'convert', '-f', 'vdi', '-O', 'raw', str(path), str(target)], check=True)
    return target


def exercise_switch(folder, iso, previous, template, qemu, firmware, data_format):
    folder.mkdir()
    data = folder / ('same-data.' + ('vdi' if data_format == 'vdi' else 'img'))
    if template:
        shutil.copyfile(template, data)
    else:
        provision(data)
    before = digest(previous)
    old_format = image_kind(previous)
    with vm(folder/'before', previous, data, qemu, firmware, image_format=old_format,
            data_format=data_format) as (q, _):
        q.wait_uart('DATA MOUNT')
        q.click(64, 270)
        q.click(64, 172)
        q.key('ctrl-n')
        q.key('a')
        q.key('ctrl-s')
        q.wait_uart('DOCUMENT SAVE')
    verify_disk(raw_data(data, data_format, folder/'before.img'), b'a')
    with vm(folder/'iso', iso, data, qemu, firmware, image_format='iso',
            data_format=data_format) as (q, f):
        q.wait_uart('DATA MOUNT')
        project_note(q, f)
        q.key('b')  # The freshly opened document has its caret at the beginning.
        q.key('ctrl-s')
        q.wait_uart('DOCUMENT SAVE')
    verify_disk(raw_data(data, data_format, folder/'after.img'), b'ba', bank=1, version=2)
    committed = digest(data)
    with vm(folder/'rollback', previous, data, qemu, firmware, image_format=old_format,
            data_format=data_format) as (q, f):
        q.wait_uart('DATA MOUNT')
        project_note(q, f)
    if digest(previous) != before or digest(data) != committed:
        raise RuntimeError('rollback read changed a disk')


def exercise(folder, image, template, qemu, firmware, boot_format, data_format):
    data = folder / ('data.' + ('vdi' if data_format == 'vdi' else 'img'))
    if template:
        shutil.copyfile(template, data)
    else:
        provision(data)
    old_boot = digest(image)
    if boot_format == 'iso':
        with vm(folder/'optical-only', image, None, qemu, firmware, image_format=boot_format,
                data_format=data_format) as (q, f):
            q.wait_uart('DATA UNAVAILABLE')
            q.click(70, f.h - 27)
            q.click(125, f.h - 54 - 273 + 121)
            q.wait_uart('WINDOW OPEN')
            processes = q.screen('processes')
            px, py = f.w // 3, (f.h - 54) // 6
            processes.check(px + 12, py, 'e6ad63')
            if not processes.contains_color(px + 16 + 7 * 11, py + 11, 11, 18, b'\xf3\xf0\xe9'):
                raise RuntimeError('the longer "Процессы" title was not drawn')
    with vm(folder/'first', image, data, qemu, firmware, image_format=boot_format,
            data_format=data_format) as (q, first):
        q.wait_uart('DATA MOUNT')
        w, h = first.w, first.h
        bx = w * 688 // 1536
        by = (h - 54) * 128 // 804
        bw = w * 756 // 1536
        first.check(bx + 12, by, 'e6ad63')
        first.check(w // 2, h - 11, '101822')
        first.check(63, 54, '496680')       # Files launcher: title bar, not a folder.
        first.check(60, 270, 'e7be80')      # Projects launcher: still a folder.
        first.check(182, h - 35, '496680')  # Files taskbar button: same window icon.
        before_x, before_y = q.x, q.y
        q.move(before_x + 25, before_y + 15)
        q.wait_uart('CURSOR DAMAGE PASS')
        moved = q.screen('cursor-only')
        boxes = [(before_x, before_y, 10, 17), (q.x, q.y, 10, 17)]
        first.expect_changes_in(moved, boxes)
        old = q.x, q.y
        q.move(bx + 150, by + 71)
        hover = q.screen('cursor-hover')
        moved.expect_changes_in(hover, [(*old, 10, 17), (q.x, q.y, 10, 17),
                                       (bx + 12, by + 54, bw - 24, 45)])
        hover.check(bx + 20, by + 65, '253143')
        hover.check(q.x + 1, q.y + 1, 'f3f0e9')  # The cursor is on top of the hovered window row.
        q.move(before_x, before_y)
        restored = q.screen('cursor-return')
        if restored.pixels != first.pixels:
            raise RuntimeError('moving out did not restore the exact original scene')
        q.click(70, h - 27)
        menu = q.screen('menu')
        menu.check(37, h - 54 - 273 + 19, '76572f')
        menu.check(46, h - 54 - 273 + 30, '496680')
        q.click(70, h - 27)

        q.move(bx + 115, by + 22)
        q.button(True)
        q.move(bx + 15, by + 66)
        q.button(False)
        q.wait_uart('WINDOW DRAG')
        bx, by = bx - 100, by + 44
        q.screen('moved').check(bx + 12, by, 'e6ad63')
        q.click(bx + bw - 109, by + 23)
        q.wait_uart('WINDOW MINIMIZE')
        q.click(230, h - 26)
        q.wait_uart('WINDOW RESTORE')
        q.click(bx + bw - 66, by + 23)
        q.screen('maximized').check(12, 0, 'e6ad63')
        q.click(w - 66, 23)
        q.screen('restored').check(bx + 12, by, 'e6ad63')

        q.click(64, 270)  # Desktop shortcut for Projects.
        q.click(64, 172)  # Notes editor.
        q.key('ctrl-n')
        for key in ('shift-s', 't', 'y', 'l', 'e', 'shift-o', 'x', 'backspace', 'shift-s', 'ret',
                    'f6', 'shift-g', 'h', 'b', 'd', 't', 'n'):
            q.key(key)
        q.key('ctrl-s')
        q.wait_uart('DOCUMENT SAVE')
        q.screen('saved')
        q.click(70, h - 27)
        q.click(125, h - 54 - 273 + 180)
        q.wait_uart('REBOOT')
        q.wait_uart('READY', count=2, timeout=26)

    with vm(folder/'second', image, data, qemu, firmware, image_format=boot_format,
            data_format=data_format) as (q, second):
        q.wait_uart('DATA MOUNT')
        project_note(q, second)
        q.screen('reopened')

    if digest(image) != old_boot:
        raise RuntimeError('system image was modified')
    verify_disk(raw_data(data, data_format, folder/'data-proof.img'))

    foreign = folder/'foreign.img'
    with foreign.open('xb') as out:
        out.write(b'not a StyleOS data disk')
        out.truncate(4096 * 512)
    foreign_hash = digest(foreign)
    with vm(folder/'foreign', image, foreign, qemu, firmware, image_format=boot_format,
            data_format='raw') as (q, _):
        q.wait_uart('DATA UNAVAILABLE')
        q.key('f3')
        q.key('a')
        q.key('ctrl-s')
        if b'STYLEOS DESKTOP DOCUMENT SAVE' in q.uart.read_bytes():
            raise RuntimeError('unrecognized disk was accepted')
    if digest(foreign) != foreign_hash:
        raise RuntimeError('unrecognized disk was written')
    return {'status': 'PASS', 'boot_format': boot_format, 'data_format': data_format,
            'boot_sha256': old_boot, 'resolution': [w, h], 'persisted_utf8': EXPECTED.decode(),
            'checks': ['live VGA and Start menu pixels', 'distinct Files icon and standard Projects folder',
                       'real PS/2 cursor changes only two small regions',
                       'cursor above the window; hover repaints only its row and cursor',
                       'PS/2 mouse move/minimize/restore/maximize',
                       'keyboard English/Cyrillic and backspace', 'save to separate IDE disk',
                       'restart from guest menu', 'reopen in fresh VM', 'disk bytes and UTF-8',
                       'unchanged system image', 'foreign disk refusal without writes'] + (
                           ['optical-only UEFI boot without a hard disk', 'live «Процессы» window']
                           if boot_format == 'iso' else [])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--boot', type=Path, help='optional already-built boot ISO, VDI or raw image')
    parser.add_argument('--data-template', type=Path, help='optional fresh data template, copied before use')
    parser.add_argument('--previous-boot', type=Path,
                        help='with ISO: check an update from and a rollback to this legacy boot VDI/raw image')
    args = parser.parse_args()
    if args.previous_boot and (not args.boot or image_kind(args.boot) != 'iso' or
                               image_kind(args.previous_boot) == 'iso'):
        parser.error('--previous-boot requires a legacy boot disk and an optical --boot ISO')
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmware):
        parser.error('OVMF firmware required')
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    if args.boot:
        image = args.boot.resolve(strict=True)
    else:
        subprocess.run([sys.executable, ROOT/'tools/build.py', '--arch', 'x86_64',
                        '--desktop-v0', '--output', folder/'build'], check=True)
        image = folder/'build/x86_64/styleos.img'
    template = args.data_template.resolve(strict=True) if args.data_template else None
    data_format = image_kind(template) if template else 'raw'
    if data_format == 'iso':
        parser.error('data template must be a VDI or a raw disk image')
    run = exercise(folder, image, template, qemu, firmware, image_kind(image), data_format)
    if args.previous_boot:
        exercise_switch(folder/'switch', image, args.previous_boot.resolve(strict=True), template,
                        qemu, firmware, data_format)
        if digest(image) != run['boot_sha256']:
            raise RuntimeError('ISO contents changed after update and rollback')
        run['previous_boot_sha256'] = digest(args.previous_boot)
        run['checks'].append('legacy VDI → ISO → legacy VDI with the same data disk and new generation')
    (folder/'results.json').write_text(json.dumps(run, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('DESKTOP V0: PASS', flush=True)


if __name__ == '__main__':
    main()
