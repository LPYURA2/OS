#!/usr/bin/env python3
"""Build identical M32 ROM, kernel, guest shell and SYSTEM.IMG disk image.
One system disk is read by guest ROM. User disk never modified by this script.
"""
import hashlib
import json
import struct
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from matryoshka import assemble, KERNEL_BASE, APP_BASE, SLOT_SIZE, APP_ORDER, CATALOG_SECTOR, APP_FIRST_SECTOR, APP_CATALOG_ENTRY_SIZE, INS_SIZE, OP_NAMES, BLOCK_SIZE, BLOCK_COUNT


def build(check=False):
    images = [('firmware', 'firmware.asm', 0),
              ('kernel', 'kernel.asm', KERNEL_BASE),
              ('shell', 'programs/shell.asm', APP_BASE)]
    records = []
    assembled = {}
    for name, rel, origin in images:
        content, _ = assemble((ROOT / rel).read_text(encoding='utf-8'), origin)
        assembled[name] = content
        dest = ROOT / 'images' / f'{name}.bin'
        if check:
            assert dest.read_bytes() == content, f'Changed or missing: {dest.name}'
        else:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(content)
        records.append({'image': dest.name, 'source': rel,
                        'load_address_hex': f'0x{origin:08x}',
                        'byte_length': len(content),
                        'sha256': hashlib.sha256(content).hexdigest()})
    kernel = assembled['kernel']
    sectors = (len(kernel) + BLOCK_SIZE - 1) // BLOCK_SIZE
    if not (1 <= sectors <= 16):
        raise ValueError('Kernel must fit within 16 sectors / 8192 bytes')
    padded = kernel.ljust(sectors * BLOCK_SIZE,b'\x00')
    checksum = sum(x[0] for x in struct.iter_unpack('<I',padded)) & 0xffffffff
    system_image = bytearray(BLOCK_COUNT*BLOCK_SIZE)
    struct.pack_into('<4sIII',system_image,0,b'MTRX',len(kernel),sectors,checksum)
    system_image[BLOCK_SIZE:BLOCK_SIZE+len(padded)] = padded
    # Guest-readable application catalog at sector 17; the guest kernel
    # validates and loads programs from sector 18 onwards. No Python preload.
    catalog = bytearray(BLOCK_SIZE)
    struct.pack_into('<4sIII', catalog, 0, b'MAPP', 1, len(APP_ORDER), 0)
    next_sector = APP_FIRST_SECTOR
    for idx, name in enumerate(APP_ORDER):
        rel = f'programs/{name}.asm'
        origin = APP_BASE + idx * SLOT_SIZE
        raw, _ = assemble((ROOT / rel).read_text(encoding='utf-8'), origin)
        if not 0 < len(raw) <= SLOT_SIZE:
            raise ValueError('Invalid program size: ' + name)
        sectors_needed = (len(raw)+BLOCK_SIZE-1)//BLOCK_SIZE
        if next_sector+sectors_needed > BLOCK_COUNT:
            raise ValueError('SYSTEM.IMG out of space')
        standalone = ROOT / 'images' / 'apps' / f'{name}.bin'
        if check:
            assert standalone.read_bytes() == raw, f'Changed or missing: {standalone.name}'
        else:
            standalone.parent.mkdir(parents=True, exist_ok=True)
            standalone.write_bytes(raw)
        padded_app = raw.ljust(sectors_needed*BLOCK_SIZE,b'\x00')
        word_sum = sum(row[0] for row in struct.iter_unpack('<I',padded_app)) & 0xffffffff
        entry_at = 16 + APP_CATALOG_ENTRY_SIZE*idx
        name_bytes = name.encode('ascii')
        if len(name_bytes) > 15: raise ValueError('Name too long')
        struct.pack_into('<16sIIIIII', catalog, entry_at,
                         name_bytes, origin, next_sector, sectors_needed,
                         len(raw), word_sum, 0)
        system_image[next_sector*BLOCK_SIZE:(next_sector+sectors_needed)*BLOCK_SIZE] = padded_app
        records.append({'image': 'apps/'+name+'.bin', 'source': rel,
                        'load_address_hex':f'0x{origin:08x}',
                        'system_disk_sector':next_sector,
                        'sector_count':sectors_needed,
                        'byte_length':len(raw),
                        'sha256':hashlib.sha256(raw).hexdigest()})
        next_sector += sectors_needed
    system_image[CATALOG_SECTOR*BLOCK_SIZE:(CATALOG_SECTOR+1)*BLOCK_SIZE] = catalog
    dest = ROOT/'images'/'system.img'
    if check:
        assert dest.read_bytes() == system_image, 'Changed or missing system.img'
    else:
        dest.write_bytes(system_image)
    records.append({'image': 'system.img','source':'generated: header + kernel.bin',
                    'load_address_hex': '0x00000000', 'byte_length':len(system_image),
                    'sha256':hashlib.sha256(system_image).hexdigest()})
    manifest = {'format': 'M32-EXPERIMENTAL-V0.3.2-GUEST-APP-LOADER',
                'instruction_bytes': INS_SIZE, 'opcode_count': len(OP_NAMES),
                'status':'TEMPORARY M32; NOT FINAL PROTEUS ISA',
                'system_disk':{'header_sector':0,'kernel_first_sector':1,
                               'kernel_sector_count':sectors,'kernel_byte_length':len(kernel),
                               'kernel_word_additive_checksum_hex':f'0x{checksum:08x}', 'app_catalog_sector':CATALOG_SECTOR, 'app_image_first_sector':APP_FIRST_SECTOR, 'app_order':list(APP_ORDER)},
                'images':records}
    out = ROOT / 'images' / 'manifest.json'
    blob = json.dumps(manifest,indent=2,ensure_ascii=False) + '\n'
    if check:
        assert out.read_text(encoding='utf-8') == blob, 'manifest mismatch'
        print('IMAGE CHECK: PASS')
    else:
        out.write_text(blob,encoding='utf-8')
        print('IMAGE BUILD: PASS')
    for rec in records:
        print(f"  {rec['image']} {rec['byte_length']} bytes sha256={rec['sha256'][:16]}...")

if __name__ == '__main__':
    build(check='--check' in sys.argv)
