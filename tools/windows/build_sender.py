#!/usr/bin/env python3
"""Build the x64 Windows package sender using Clang and the Win32 DLL ABI."""
import argparse
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build import tool

EXPORTS = {
    'kernel32': 'CreateFileW GetFileSizeEx ReadFile WriteFile CloseHandle WaitNamedPipeW PeekNamedPipe '
                'CreateEventW ResetEvent GetOverlappedResultEx CancelIoEx GetLastError ExitProcess',
    'comdlg32': 'GetOpenFileNameW',
    'user32': 'MessageBoxW'
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    out = p.parse_args().output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = tool('CLANG', ['clang', 'clang-19'])
    dlltool = tool('DLLTOOL', ['llvm-dlltool', 'llvm-dlltool-19'])
    ld = tool('LLD_LINK', ['lld-link', 'lld-link-19'])
    libs = []
    for dll, names in EXPORTS.items():
        src, lib = out / (dll + '.def'), out / (dll + '.lib')
        src.write_text(f'LIBRARY {dll}.dll\nEXPORTS\n' + '\n'.join(names.split()) + '\n')
        subprocess.run([dlltool, '-m', 'i386:x86-64', '-d', src, '-l', lib], check=True)
        libs.append(lib)
    obj = out / 'sender.obj'
    subprocess.run([cc, '--target=x86_64-pc-win32-coff', '-O2', '-std=c11',
                    '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
                    '-Wall', '-Wextra', '-Werror', '-c',
                    Path(__file__).with_name('styleos_send.c'), '-o', obj], check=True)
    exe = out / 'Send-StyleOS-Update.exe'
    subprocess.run([ld, '/machine:x64', '/subsystem:windows,6.02', '/entry:start',
                    '/nodefaultlib', '/dynamicbase', '/nxcompat', '/timestamp:0',
                    f'/out:{exe}', obj, *libs], check=True)
    print(exe)


if __name__ == '__main__':
    main()
