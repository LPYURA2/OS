#!/usr/bin/env python3
"""Read-only forensic inventory of *embedded*, not installable, .tea-like data.

This is intentionally not a ZIP repair utility. It never changes a boot image,
executes an ELF payload, or certifies a package as reinstallable. It checks the
manifest, ELF bytes, and checksums at the offsets actually present in the EFI.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zlib

HEADER = struct.Struct('<IHHHHHIIIHH')
LOCAL_MAGIC = b'PK\x03\x04'
MAX_IMAGE = 32 * 1024 * 1024
MAX_MANIFEST = 8192
MAX_ELF = 4 * 1024 * 1024


def scan(data: bytes):
    if len(data) > MAX_IMAGE:
        raise ValueError('EFI image exceeds audit limit')
    records = []
    for match in re.finditer(re.escape(LOCAL_MAGIC), data):
        pos = match.start()
        if pos + HEADER.size > len(data):
            continue
        _, version, flags, method, _time, _date, crc, size, raw_size, name_len, extra_len = HEADER.unpack_from(data, pos)
        after_name = pos + HEADER.size + name_len
        payload_start = after_name + extra_len
        if (flags or method or version > 63 or name_len != len(b'manifest.json') or
                data[pos + HEADER.size:after_name] != b'manifest.json' or
                size != raw_size or size > MAX_MANIFEST or payload_start + size > len(data)):
            continue
        manifest_raw = data[payload_start:payload_start + size]
        try:
            manifest = json.loads(manifest_raw.decode('utf-8'))
        except (UnicodeError, ValueError):
            continue
        if not isinstance(manifest, dict) or manifest.get('format') != 'styleos.tea.sdk-v0':
            continue
        project = manifest.get('project')
        payload_manifest = manifest.get('payload')
        if not isinstance(project, dict) or not isinstance(payload_manifest, dict):
            continue
        pid = project.get('id')
        if not isinstance(pid, str) or not re.fullmatch(r'[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+', pid):
            continue
        entry = payload_start + size
        expected_name = b'payload.elf'
        if entry + HEADER.size + len(expected_name) > len(data) or data[entry:entry+4] != LOCAL_MAGIC:
            continue
        _sig, pv, pf, pm, _t, _d, pcrc, plen, praw, nlen, xlen = HEADER.unpack_from(data, entry)
        elf_start = entry + HEADER.size + nlen + xlen
        if (pf or pm or plen != praw or plen > MAX_ELF or plen < 64 or
                nlen != len(expected_name) or data[entry+HEADER.size:entry+HEADER.size+nlen] != expected_name or
                elf_start + plen > len(data)):
            continue
        elf = data[elf_start:elf_start + plen]
        digest = hashlib.sha256(elf).hexdigest()
        expected = payload_manifest.get('sha256')
        if not isinstance(expected, str) or not re.fullmatch('[0-9a-f]{64}', expected):
            continue
        records.append({
            'project_id': pid,
            'project_name': project.get('name'),
            'interface': manifest.get('interface'),
            'manifest_offset': pos,
            'elf_offset': elf_start,
            'elf_bytes': plen,
            'manifest_crc_valid': zlib.crc32(manifest_raw) == crc,
            'elf_crc_valid': zlib.crc32(elf) == pcrc,
            'elf_sha256_valid': digest == expected,
            'elf_sha256_observed': digest,
            'elf_sha256_manifest': expected,
            'extractable_as_verified_package': False,
        })
    return records


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('efi', type=Path, help='existing BOOTX64.EFI (read only)')
    p.add_argument('--output', type=Path)
    args = p.parse_args(argv)
    data = args.efi.read_bytes()
    items = scan(data)
    report = {'source': str(args.efi), 'sha256': hashlib.sha256(data).hexdigest(),
              'interpretation': 'byte-level audit only; no executable/installable app recovered',
              'packages': items}
    encoded = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding='utf-8')
    else:
        sys.stdout.write(encoded)
    if not any(x['project_id'] == 'styleos.documents' for x in items):
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
