#!/usr/bin/env python3
"""Exercise StyleFS via real PS/2 events and a secondary IDE data VDI in QEMU."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import zipfile

from test_desktop_v0 import command_exists, digest, firmware_path, vm


def command(*args):
    return subprocess.check_output(list(map(str, args)), text=True)


def file_box(frame):
    return frame.w * 688 // 1536, (frame.h - 54) * 128 // 804


def row(q, b, index):
    q.click(b[0] + 105, b[1] + 120 + index * 31)


def activate(q, b, index):
    row(q, b, index)
    q.key('ret')


def keys(q, sequence):
    for key in sequence:
        q.key(key)


def run(folder, iso, clean, image_tool, qemu, firmware, converter):
    iso_sha, clean_sha = digest(iso), digest(clean)
    seed = folder / 'seed.raw'
    data = folder / 'data.vdi'
    foreign = folder / 'foreign.raw'
    fixture = folder / 'Foreign.docx'
    # A small external OOXML fixture. The guest must not interpret it as text.
    with zipfile.ZipFile(fixture, 'x', zipfile.ZIP_DEFLATED) as z:
        def member(name, data):
            z.writestr(zipfile.ZipInfo(name), data)
        member('[Content_Types].xml', '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
               '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
               '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
               '</Types>')
        member('_rels/.rels', '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
               '</Relationships>')
        member('word/document.xml', '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
               '<w:body><w:p><w:r><w:t>Office round trip</w:t></w:r></w:p></w:body></w:document>')
    command(converter, 'convert', '-f', 'vdi', '-O', 'raw', clean, seed)
    command(image_tool, 'put', seed, '/Загрузки/Foreign.docx', fixture)
    command(converter, 'convert', '-f', 'raw', '-O', 'vdi', seed, data)

    with vm(folder/'first', iso, data, qemu, firmware, image_format='iso', data_format='vdi') as (q, first):
        q.wait_uart('STYLEFS MOUNT')
        b = file_box(first)
        x, y = q.x, q.y
        q.move(x + 18, y + 12)
        q.wait_uart('CURSOR DAMAGE PASS')
        first.expect_changes_in(q.screen('stylefs-cursor'), [(x, y, 10, 17), (q.x, q.y, 10, 17)])
        q.click(64, 270)  # Projects desktop shortcut.
        q.click(b[0] + 107, b[1] + 65)
        keys(q, ('l', 'a', 'b', 'ret'))
        q.wait_uart('STYLEFS MKDIR')
        activate(q, b, 0)
        q.click(b[0] + 195, b[1] + 65)  # + Текст
        keys(q, ('shift-s', 't', 'y', 'l', 'e', 'ret', 'f6', 'shift-g', 'h', 'b', 'd', 't', 'n', 'ctrl-s', 'f6'))
        q.wait_uart('DOCUMENT SAVE')
        q.key('f2')  # Bring Files to the front.
        row(q, b, 0)
        q.key('f2')  # Rename the selected file.
        keys(q, ('backspace',) * 9 + ('r','e','p','o','r','t','dot','t','x','t','ret'))
        q.wait_uart('STYLEFS RENAME')
        q.screen('stylefs-report')
        keys(q, ('backspace', 'backspace'))
        activate(q, b, 2)  # Загрузки
        activate(q, b, 0)  # Foreign.docx, no action handler
        q.wait_uart('STYLEFS NO HANDLER')
        keys(q, ('ctrl-c', 'backspace'))
        activate(q, b, 0)  # Документы
        q.key('ctrl-v')
        q.wait_uart('STYLEFS COPY')
        q.screen('stylefs-copied')
        q.click(70, first.h - 27)
        q.click(125, first.h - 54 - 273 + 180)
        q.wait_uart('REBOOT')
        q.wait_uart('READY', count=2, timeout=26)

    with vm(folder/'restart', iso, data, qemu, firmware, image_format='iso', data_format='vdi') as (q, f):
        q.wait_uart('STYLEFS MOUNT')
        b = file_box(f)
        q.click(64, 270)
        activate(q, b, 0)
        activate(q, b, 0)
        q.wait_uart('DOCUMENT OPEN')
        q.screen('stylefs-reopened')

    decoded = folder / 'verified.raw'
    command(converter, 'convert', '-f', 'vdi', '-O', 'raw', data, decoded)
    for path, host in (('/Загрузки/Foreign.docx', 'original.docx'),
                       ('/Документы/Foreign.docx', 'copied.docx'),
                       ('/Проекты/lab/report.txt', 'report.txt')):
        command(image_tool, 'get', decoded, path, folder / host)
    if not (folder / 'copied.docx').read_bytes() == (folder / 'original.docx').read_bytes() == fixture.read_bytes():
        raise RuntimeError('copy or original differs from the imported DOCX')
    if (folder / 'report.txt').read_bytes() != 'Style\nПривет'.encode('utf-8'):
        raise RuntimeError('wrong persisted UTF-8 text')
    if 'Текст.txt' in command(image_tool, 'list', decoded, '/Проекты/lab'):
        raise RuntimeError('old name survived the rename')

    with foreign.open('xb') as f:
        f.write(b'STYLEFS1' + b'invalid superblock')
        f.truncate(8 * 1024 * 1024)
    initial = digest(foreign)
    with vm(folder/'foreign', iso, foreign, qemu, firmware, image_format='iso', data_format='raw') as (q, _):
        q.wait_uart('STYLEFS REFUSED')
        q.wait_uart('DATA UNAVAILABLE')
        keys(q, ('ctrl-n', 't', 'ret', 'f3', 'a', 'ctrl-s'))
    if digest(foreign) != initial or digest(iso) != iso_sha or digest(clean) != clean_sha:
        raise RuntimeError('a rejected disk or a source artifact was modified')
    return {'status': 'PASS', 'boot_sha256': iso_sha, 'clean_vdi_sha256': clean_sha,
            'foreign_docx_sha256': digest(fixture), 'guest_text': 'Style\nПривет',
            'checks': ['StyleFS VDI mount and cursor damage', 'nested directory creation via the Files UI',
                       'UTF-8 editor save and rename via UI', 'copy foreign DOCX without opening it',
                       'read the same UTF-8 document after restart and in a fresh VM',
                       'copied DOCX exactly equals host bytes; original remains identical',
                       'corrupt same-size disk refused; ISO and source data template unchanged']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('boot', 'data-template', 'image-tool', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args()
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    converter = command_exists(os.environ.get('QEMU_IMG', 'qemu-img'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmware):
        p.error('OVMF firmware required')
    folder = a.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    result = run(folder, a.boot.resolve(strict=True), a.data_template.resolve(strict=True),
                 a.image_tool.resolve(strict=True), qemu, firmware, converter)
    (folder/'results.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('STYLEFS DESKTOP: PASS', flush=True)


if __name__ == '__main__':
    main()
