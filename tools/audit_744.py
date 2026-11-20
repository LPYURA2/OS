#!/usr/bin/env python3
"""Read-only structural audit of the StyleOS 744 original ISO.

This does not reconstruct source, validate application behavior or claim a VM boot.
It confirms ISO9660+FAT ESP metadata and exact EFI byte equality to the ISO copy.
No firmware is executed and no file is modified.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

BLOCK = 2048


def le16(raw, off):
    return struct.unpack_from('<H', raw, off)[0]


def le32(raw, off):
    return struct.unpack_from('<I', raw, off)[0]


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def iso_files(raw):
    if len(raw) < BLOCK * 20 or raw[16 * BLOCK + 1:16 * BLOCK + 6] != b'CD001':
        raise ValueError('not ISO9660 with a primary volume descriptor')
    root = raw[16 * BLOCK + 156:16 * BLOCK + 190]
    seen = set()
    found = {}

    def walk(rec, prefix=''):
        block = le32(rec, 2)
        size = le32(rec, 10)
        if (block, size) in seen:
            return
        seen.add((block, size))
        if block * BLOCK + size > len(raw):
            raise ValueError('directory outside ISO')
        for sec in range(block * BLOCK, block * BLOCK + size, BLOCK):
            chunk = raw[sec:sec + BLOCK]
            pos = 0
            while pos < BLOCK and chunk[pos]:
                length = chunk[pos]
                if length < 34 or pos + length > BLOCK:
                    raise ValueError('invalid directory record')
                ent = chunk[pos:pos + length]
                pos += length
                name = ent[33:33 + ent[32]].decode('ascii', 'replace')
                if name in ('\x00', '\x01'):
                    continue
                name = name.split(';', 1)[0]
                path = prefix + '/' + name
                off = le32(ent, 2) * BLOCK
                n = le32(ent, 10)
                if off + n > len(raw):
                    raise ValueError('file outside ISO')
                if ent[25] & 2:
                    walk(ent, path)
                else:
                    found[path] = raw[off:off + n]
    walk(root)
    return found


def esp_files(esp):
    if len(esp) < 1024 or esp[510:512] != b'\x55\xaa':
        raise ValueError('invalid FAT boot sector')
    sec = le16(esp, 11)
    spc = esp[13]
    reserved = le16(esp, 14)
    nfats = esp[16]
    roots = le16(esp, 17)
    fatsec = le16(esp, 22)
    total = le16(esp, 19) or le32(esp, 32)
    if sec != 512 or not (1 <= spc <= 128) or not (1 <= nfats <= 2) or not fatsec or total * sec > len(esp):
        raise ValueError('unexpected FAT16 parameters')
    root_off = (reserved + nfats * fatsec) * sec
    data_off = root_off + roots * 32
    fat = esp[reserved * sec:(reserved + fatsec) * sec]
    result = {}

    def contents(cluster, length):
        out = bytearray()
        seen = set()
        while 2 <= cluster < 0xfff8 and cluster not in seen:
            seen.add(cluster)
            offset = data_off + (cluster - 2) * sec * spc
            if offset + sec * spc > len(esp):
                raise ValueError('invalid FAT cluster')
            out += esp[offset:offset + sec * spc]
            if cluster * 2 + 2 > len(fat):
                raise ValueError('invalid FAT chain')
            cluster = le16(fat, cluster * 2)
        return bytes(out[:length])

    def walk(directory, prefix=''):
        for pos in range(0, len(directory), 32):
            ent = directory[pos:pos + 32]
            if len(ent) < 32 or ent[0] == 0:
                break
            if ent[0] == 0xe5 or ent[11] in (8, 15):
                continue
            name = ent[:8].decode('ascii', 'replace').strip()
            ext = ent[8:11].decode('ascii', 'replace').strip()
            if name in ('.', '..'):
                continue
            path = prefix + '/' + name + (('.' + ext) if ext else '')
            cluster = le16(ent, 26)
            size = le32(ent, 28)
            if ent[11] & 16:
                walk(contents(cluster, 256 * sec * spc), path)
            else:
                result[path] = contents(cluster, size)
    walk(esp[root_off:root_off + roots * 32])
    return result


def audit(path):
    raw = Path(path).read_bytes()
    files = iso_files(raw)
    esp = files.get('/EFI_BOOT.IMG')
    efi = files.get('/EFI/BOOT/BOOTX64.EFI')
    if not esp or not efi:
        raise ValueError('EFI boot files missing')
    fat = esp_files(esp)
    efi_in_fat = fat.get('/EFI/BOOT/BOOTX64.EFI')
    if efi != efi_in_fat:
        raise ValueError('ISO and embedded ESP contain DIFFERENT EFI binaries')
    return {
        'iso_sha256': sha(raw),
        'iso_bytes': len(raw),
        'iso_paths': sorted(files),
        'esp_paths': sorted(fat),
        'efi_sha256': sha(efi),
        'efi_bytes': len(efi),
        'iso_and_esp_efi_identical': True,
        'documents_tea_mentioned_in_boot_executable': b'Documents.tea' in efi,
        'standalone_documents_tea_file_in_installer': any(p.lower().endswith('/documents.tea') for p in list(files) + list(fat)),
        'limitation': 'No source code or real app test is established by binary inspection.'
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument('iso', type=Path)
    args = p.parse_args()
    print(json.dumps(audit(args.iso), ensure_ascii=False, sort_keys=True, indent=2))

if __name__ == '__main__':
    main()
