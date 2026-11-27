"""Build external acceptance packages. Only their identities enter the EFI image."""
import hashlib
from pathlib import Path
import subprocess
import zipfile

from tea_package import pack

ROOT = Path(__file__).resolve().parents[1]

def build(folder):
    header = []
    for role in ('source', 'target'):
        name = 'Drop-' + role
        obj, elf, tea = (folder / (name + suffix) for suffix in ('.o', '.elf', '.tea'))
        defines = ['-DDD_SOURCE=1'] if role == 'source' else []
        subprocess.run(['clang', '--target=x86_64-unknown-linux-gnu', '-c', '-nostdlib',
                        '-I', str(ROOT / 'include'), *defines, str(ROOT / 'tests/guest/deep_drop_72.S'),
                        '-o', str(obj)], check=True)
        subprocess.run(['ld.lld', '-nostdlib', '--strip-all', '--build-id=none',
                        '-T', str(ROOT / 'apps/tea_768.ld'), '-e', 'dd_start',
                        '-o', str(elf), str(obj)], check=True)
        if tea.exists():
            tea.unlink()
        pack(tea, elf, 'styleos.drop_' + role, name, '0.1.0', 'x86_64')
        with zipfile.ZipFile(tea) as package:
            manifest = package.read('manifest.json')
        blob = tea.read_bytes()
        prefix = 'drop72_' + role
        header.append(f'#define {prefix}_bytes {len(blob)}u')
        for label, data in [('pkg_hash', hashlib.sha256(blob).digest()),
                            ('elf_hash', hashlib.sha256(elf.read_bytes()).digest()),
                            ('manifest', manifest)]:
            header.append(f'static const uint8_t {prefix}_{label}[]={{' +
                          ','.join(str(byte) for byte in data) + '};')
    (folder / 'deep_drop_pins.h').write_text('\n'.join(header) + '\n')
