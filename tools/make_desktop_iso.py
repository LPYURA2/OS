#!/usr/bin/env python3
"""Package the existing x64 desktop EFI executable as a UEFI El Torito ISO."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ESP_BYTES = 16 * 1024 * 1024
EPOCH = '946684800'  # 2000-01-01, accepted by FAT and ISO date fields.
ISO_STAMP = '2000010100000000'


def create_iso(efi, output):
    efi = efi.resolve(strict=True)
    if not efi.is_file() or not 0 < efi.stat().st_size < ESP_BYTES:
        raise ValueError('expected a non-empty EFI executable smaller than the boot partition')
    if efi == output.resolve() or (output.exists() and output.samefile(efi)):
        raise ValueError('the EFI executable and output ISO must be different files')
    tools = ('mkfs.fat', 'mmd', 'mcopy', 'xorriso')
    missing = [tool for tool in tools if not shutil.which(tool)]
    if missing:
        raise ValueError('required tool(s) not found: ' + ', '.join(missing))

    output.parent.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, 'SOURCE_DATE_EPOCH': EPOCH, 'TZ': 'UTC', 'LC_ALL': 'C', 'MTOOLSRC': os.devnull}
    with tempfile.TemporaryDirectory(prefix='.desktop-iso-', dir=output.parent) as temp:
        staging = Path(temp)
        tree = staging / 'tree'
        tree.mkdir()
        esp = tree / 'EFI_BOOT.IMG'
        with esp.open('xb') as partition:
            partition.truncate(ESP_BYTES)
        subprocess.run(['mkfs.fat', '--invariant', '-F', '16', '-n', 'STYLEOS', str(esp)],
                       env=env, check=True)
        for directory in ('::/EFI', '::/EFI/BOOT'):
            subprocess.run(['mmd', '-i', str(esp), directory], env=env, check=True)
        subprocess.run(['mcopy', '-i', str(esp), str(efi), '::/EFI/BOOT/BOOTX64.EFI'],
                       env=env, check=True)
        # Also expose the executable in the ordinary ISO directory tree.
        boot_dir = tree / 'EFI' / 'BOOT'
        boot_dir.mkdir(parents=True)
        shutil.copyfile(efi, boot_dir / 'BOOTX64.EFI')
        candidate = staging / 'StyleOS.iso'
        subprocess.run(['xorriso', '-as', 'mkisofs', '-V', 'STYLEOS_DESKTOP',
                        '--modification-date=' + ISO_STAMP, '--set_all_file_dates', ISO_STAMP,
                        '-c', 'BOOT.CAT', '-e', esp.name, '-no-emul-boot',
                        '-o', str(candidate), str(tree)], env=env, check=True)
        os.replace(candidate, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--efi', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        create_iso(args.efi, args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'ISO image error: {error}\n')
    print(f'UEFI ISO: {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
