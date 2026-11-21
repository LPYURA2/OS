#!/usr/bin/env python3
"""Create external raw IDE image for the canonical package; keep ELF out of EFI.

This is a narrow read-only physical-disk fixture, NOT a release installer.
"""
import hashlib
import json
from pathlib import Path
import struct
import zipfile

SIZE = 128 * 512

def build(output: Path):
    output.mkdir(parents=True, exist_ok=True)
    package = (output / 'minimal.tea').read_bytes()
    site = json.loads((output / 'tea-768-site.json').read_text())
    if not 22 <= len(package) <= 16384 or hashlib.sha256(package).hexdigest() != site['package_sha256']:
        raise ValueError('canonical package not verified')
    with zipfile.ZipFile(output / 'minimal.tea') as z:
        manifest = z.read('manifest.json')
        elf = z.read('bin/x86_64/app.elf')
    if hashlib.sha256(elf).hexdigest() != site['elf_sha256']:
        raise ValueError('ELF was not validated')
    hdr = bytearray(512)
    hdr[:8] = b'STEA769\0'
    struct.pack_into('<II', hdr, 8, 1, len(package))
    hdr[16:48] = bytes.fromhex(site['package_sha256'])
    disk = bytearray(SIZE)
    disk[:512] = hdr
    disk[512:512+len(package)] = package
    (output / 'native-tea-769-disk.img').write_bytes(disk)
    header = [
        '/* Generated 769 pinned metadata: NO embedded ELF / .tea payload. */',
        '#ifndef STYLEOS_TEA_769_EXPECTED_H',
        '#define STYLEOS_TEA_769_EXPECTED_H',
        '#define TEA_769_PKG_SHA "'+site['package_sha256']+'"',
        '#define TEA_769_ELF_SHA "'+site['elf_sha256']+'"',
        f'#define TEA_769_MANIFEST_SIZE {len(manifest)}u',
        '#define TEA_769_PACKAGE_SIZE '+str(len(package))+'u',
    ]
    for name, off in site['symbols'].items():
        header.append('#define TEA_768_'+name.removeprefix('tea_768_').upper()+'_OFFSET '+str(off)+'u')
    def bytes_array(name,data):
        return 'static const unsigned char '+name+'['+str(len(data))+']={' + ','.join('0x%02x'%b for b in data) + '};'
    header += [bytes_array('tea_769_pkg_sha',bytes.fromhex(site['package_sha256'])),
               bytes_array('tea_769_elf_sha',bytes.fromhex(site['elf_sha256'])),
               bytes_array('tea_769_expected_manifest',manifest),'#endif']
    (output / 'tea_769_expected.h').write_text('\n'.join(header)+'\n')
    info = {'image_sha256': hashlib.sha256(disk).hexdigest(),
            'package_sha256':site['package_sha256'],
            'elf_sha256':site['elf_sha256'],
            'package_bytes':len(package), 'disk_bytes':len(disk),
            'sectors':128, 'app_symbols':site['symbols'],
            'package_on_disk_at_lba':1, 'guest_extracts_package':True,
            'efi_embeds_elf':False}
    (output / 'tea-769-site.json').write_text(json.dumps(info,indent=2)+'\n')
    return info

if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    print(build(p.parse_args().output))
