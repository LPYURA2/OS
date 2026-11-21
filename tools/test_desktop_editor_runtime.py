#!/usr/bin/env python3
"""Host verification: editor state stays alive when the shell is rebuilt."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = os.environ.get('CC', 'cc')
    if not shutil.which(compiler):
        raise RuntimeError(f'Host compiler unavailable: {compiler}')
    with tempfile.TemporaryDirectory(prefix='styleos-editor-state-') as out:
        binary = Path(out) / 'editor_runtime'
        subprocess.run([
            compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
            '-I', str(ROOT / 'include'),
            str(ROOT / 'tests/test_desktop_editor_runtime.c'),
            str(ROOT / 'kernel/desktop_editor_runtime.c'),
            '-o', str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print('DESKTOP EDITOR RUNTIME: PASS')


if __name__ == '__main__':
    main()
