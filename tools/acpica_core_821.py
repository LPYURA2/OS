#!/usr/bin/env python3
"""S821: compile the pinned real ACPICA interpreter set to x86-64 COFF.

This is a build/link admission gate, NOT proof of guest AML execution. No
unverified source, internet, or host OS library is admitted in this helper.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

CORE_DIRS = (
    'dispatcher', 'events', 'executer', 'hardware', 'namespace',
    'parser', 'resources', 'tables', 'utilities',
)
MINIMUM = {"dispatcher": 15, "events": 14, "executer": 26,
           "hardware": 9, "namespace": 21, "parser": 10,
           "resources": 12, "tables": 8, "utilities": 39}
# ACPICA resource dump tables are built only for debug/disassembly modes.
OPTIONAL_DEBUG_ONLY = {"resources/rsdump.c", "resources/rsdumpinfo.c"}
ENTRYPOINTS = (
    'AcpiInitializeSubsystem', 'AcpiLoadTables', 'AcpiEvaluateObject',
    'AcpiGetDevices', 'AcpiUtChecksum',
)
EXPECTED_SHA = 'aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5'


def select(source: Path, manifest: dict):
    if (manifest.get('version') != '20260930' or
        manifest.get('archive_sha256') != EXPECTED_SHA or
        manifest.get('all_sources_verified') is not True or
        manifest.get('guest_acpica_linked') is not False or
        manifest.get('dma_authorized') is not False):
        raise ValueError('ACPICA manifest does not attest to the pinned, verified source')
    if not source.is_dir() or not (source / 'include/acpi.h').is_file():
        raise ValueError('verified ACPICA source is missing')
    result = []
    for directory in CORE_DIRS:
        p = source / 'components' / directory
        files = [f for f in sorted(p.glob('*.c'))
                 if f.relative_to(source/'components').as_posix() not in OPTIONAL_DEBUG_ONLY]
        if len(files) < MINIMUM[directory]:
            raise ValueError('incomplete component directory: ' + directory)
        for f in files:
            if not f.is_file() or f.is_symlink() or not f.resolve().is_relative_to(source.resolve()) or f.stat().st_size > 1_000_000:
                raise ValueError('unsafe ACPICA source file: ' + f.name)
            result.append(f)
    if not 160 <= len(result) <= 190:
        raise ValueError('unexpected ACPICA core size')
    return result


def build(source, manifest, output, cc, ar, objdump):
    files = select(source, manifest)
    output.mkdir(parents=True, exist_ok=True)
    objects = []
    logs = []
    for i, f in enumerate(files):
        obj = output / ('core_%03d.o' % i)
        cmd = [cc, '--target=x86_64-pc-win32-coff', '-std=c11', '-O2',
               '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
               '-fno-asynchronous-unwind-tables', '-mno-red-zone',
               '-ffunction-sections', '-fdata-sections',
               '-D_GNU_EFI', '-DACPI_MACHINE_WIDTH=64', '-include', str(Path(__file__).resolve().parents[1]/'include/styleos/acpica_efi_prelude_821.h'), '-I', str(source/'include'),
               '-c', str(f), '-o', str(obj)]
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode:
            raise RuntimeError('ACPICA freestanding COFF compile failed: ' + f.relative_to(source).as_posix() + '\n' + p.stderr[-2000:])
        logs.append({'file': f.relative_to(source).as_posix(),
                     'source_sha256': hashlib.sha256(f.read_bytes()).hexdigest(),
                     'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest()})
        objects.append(obj)
    lib = output/'libacpica-core-821.lib'
    subprocess.run([ar,'rcs',str(lib), *map(str,objects)],check=True)
    # Inspect real exported/defined COFF symbols: not just filenames or a linker stub.
    present = set()
    for obj in objects:
        p = subprocess.run([objdump, '-t', str(obj)],check=True,capture_output=True,text=True)
        for s in ENTRYPOINTS:
            if s in p.stdout:
                present.add(s)
    if present != set(ENTRYPOINTS):
        raise RuntimeError('ACPICA core entrypoint missing: '+str(sorted(set(ENTRYPOINTS)-present)))
    result = {'stage':821, 'official_version':'20260930',
              'upstream_sha256':EXPECTED_SHA, 'language':'C',
              'target':'x86_64-pc-win32-coff', 'core_source_count':len(files),
              'entrypoints_present':sorted(present), 'objects':logs,
              'archive_sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),
              'full_guest_interpreter_executed':False,
              'guest_aml_executed':False,'dma_authorized':False,
              'foundation_complete':False}
    (output/'core-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print('STYLEOS S821 FULL ACPICA CORE COFF PASS',len(files),'modules')
    return result


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--verified-source',required=True,type=Path)
    ap.add_argument('--source-manifest',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args()
    tools=[]
    for name, options in [('compiler',['clang']),('archive',['llvm-ar','llvm-ar-19','llvm-ar-18','llvm-ar-17','llvm-ar-16','ar']),('symbols',['llvm-objdump','llvm-objdump-19','llvm-objdump-18','objdump'])]:
        tool=next((shutil.which(x) for x in options if shutil.which(x)),None)
        if not tool:raise SystemExit('missing '+name)
        tools.append(tool)
    build(args.verified_source,json.loads(args.source_manifest.read_text()),args.output,*tools)
if __name__=='__main__':main()
