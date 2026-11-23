#!/usr/bin/env python3
"""Stage 817: ACPICA AcpiExec replay of genuine QEMU tables, host only."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import resource
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from acpica_replay_817_verify import (MODES,check_results,classify_hardware,
                                     synthetic_evaluation_ok,validate_table)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def limits():
    resource.setrlimit(resource.RLIMIT_CPU,(20,20))
    resource.setrlimit(resource.RLIMIT_AS,(512*1024*1024,512*1024*1024))
    resource.setrlimit(resource.RLIMIT_FSIZE,(2*1024*1024,2*1024*1024))
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def bounded(argv,cwd,timeout=35):
    return subprocess.run(argv,cwd=cwd,capture_output=True,text=True,
                          check=False,timeout=timeout,preexec_fn=limits)
def compile_asl(iasl,source,out):
    out.parent.mkdir(parents=True,exist_ok=True)
    p=bounded([iasl,'-p',str(out),str(source)],out.parent)
    (out.parent/(out.name+'.iasl.log')).write_text(p.stdout+'\n'+p.stderr)
    aml=out.with_suffix('.aml')
    sig=b'DSDT' if source.stem=='dsdt' else b'SSDT'
    if p.returncode or not aml.exists() or not validate_table(aml.read_bytes(),sig):
        raise RuntimeError('S817 iasl fixture failure '+p.stderr[-800:])
    return aml
def replay(exe,methods,tables,dst,label):
    batch=';'.join('evaluate '+m for m in methods)
    p=bounded([exe,'-di','-do','-b',batch,*map(str,tables)],dst,timeout=40)
    log=p.stdout+'\n'+p.stderr
    (dst/(label+'.acpica.log')).write_text(log)
    return p.returncode,log
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',required=True,type=Path)
    output=parser.parse_args().output.resolve()
    output.mkdir(parents=True,exist_ok=True)
    exe=shutil.which('acpiexec');iasl=shutil.which('iasl')
    if not exe or not iasl:
        raise SystemExit('S817 NOT RUN: acpica-tools missing')
    v=bounded([exe,'-?'],output)
    banner=v.stdout+v.stderr
    if 'AML Execution/Debug Utility' not in banner:
        raise SystemExit('S817 FAIL: unexpected ACPICA binary')
    (output/'acpiexec-version.txt').write_text(banner)
    (output/'acpiexec-sha256.txt').write_text(sha(Path(exe))+' '+exe+'\n')
    (output/'iasl-sha256.txt').write_text(sha(Path(iasl))+' '+iasl+'\n')
    fixture=output/'fixture';fixture.mkdir(exist_ok=True)
    d=compile_asl(iasl,ROOT/'tests/acpi817/dsdt.asl',fixture/'dsdt')
    s=compile_asl(iasl,ROOT/'tests/acpi817/ssdt.asl',fixture/'ssdt')
    rc,log=replay(exe,(r'\_SB.PCI0.X817',r'\_SB.PCI0.S817'),[d,s],fixture,'synthetic')
    if rc!=0 or not synthetic_evaluation_ok(log):
        raise SystemExit('S817 FAIL: ACPICA DSDT/SSDT sentinel methods not proven')
    hardware=output/'hardware816'
    with (output/'hardware816-build.log').open('w') as log:
        p=subprocess.run([sys.executable,str(ROOT/'tools/test_acpi_namespace_816.py'),
                          '--output',str(hardware)],cwd=ROOT,stdout=log,
                         stderr=subprocess.STDOUT,check=False,timeout=180)
    if p.returncode:
        raise SystemExit('S817 FAIL: QEMU/OVMF S816 hardware capture failed')
    hw=json.loads((hardware/'results.json').read_text())
    if hw.get('stage')!=816 or hw.get('status')!='PASS':
        raise SystemExit('S817 FAIL: invalid QEMU provenance')
    if any(hw.get(k) is not False for k in
           ('acpi_namespace_evaluated','dma_access_authorized','production_cr3_authorized')):
        raise SystemExit('S817 FAIL: untrusted hardware authorization claim')
    observations={};success=True
    for mode in MODES:
        entry=hw['modes'][mode]
        if mode=='amd' and entry.get('status')=='SKIPPED':
            observations[mode]={'status':'SKIPPED','reason':'QEMU AMD IOMMU unavailable'}
            continue
        if entry.get('status')!='PASS' or entry.get('qemu_exit_code')!=33:
            raise SystemExit('S817 FAIL: QEMU provenance '+mode)
        folder=hardware/mode
        uart=(folder/'serial.log').read_bytes()
        if hashlib.sha256(uart).hexdigest()!=entry['uart_sha256']:
            raise SystemExit('S817 FAIL: original UART SHA '+mode)
        tables=[folder/'dsdt.aml']+sorted(folder.glob('ssdt-*.aml'))
        for i,t in enumerate(tables):
            if not validate_table(t.read_bytes(),b'DSDT' if i==0 else b'SSDT'):
                raise SystemExit('S817 FAIL: original firmware ACPI table '+mode)
        rc,log=replay(exe,(r'\_SB.PCI0._SEG',r'\_SB.PCI0._BBN',
                           r'\_SB.PCI0._CRS'),tables,folder,'real')
        observed=classify_hardware(mode,entry['qemu_exit_code'],uart,log,
                                   entry['uart_sha256'],acpica_returncode=rc)
        observations[mode]={**observed,'qemu_exit_code':entry['qemu_exit_code'],
            'uart_sha256':entry['uart_sha256'],'dsdt_sha256':sha(tables[0]),
            'ssdt_sha256':[sha(t) for t in tables[1:]],
            'acpiexec_log_sha256':hashlib.sha256(log.encode()).hexdigest(),
            'acpiexec_exit_code':rc,'buses':entry['observations'].get('buses'),
            'bridges':entry['observations'].get('bridges')}
        if observed['status']!='REPLAY_ONLY':success=False
        print('S817 ACPICA',mode,observed['status'],entry['qemu_exit_code'],flush=True)
    result={'stage':817,'status':'PASS' if success else 'FAIL',
        'source_baseline_sha':'99e8d873aba5725e77760d0360280763c0d14fb9',
        'acpiexec_tool_sha256':sha(Path(exe)),'iasl_tool_sha256':sha(Path(iasl)),
        'acpica_synthetic_methods_verified':True,'replay_uses_acpiexec':True,
        'guest_acpica_integrated':False,'acpi_crs_hardware_verified':False,
        'acpi_seg_hardware_verified':False,'acpi_bbn_hardware_verified':False,
        'ecam_reservation_verified':False,'pci_segments_complete':False,
        'iommu_ownership_complete':False,'dma_domains_verified':False,
        'dma_access_authorized':False,'dma_isolation_verified':False,
        'uefi_runtime_verified':False,'production_cr3_authorized':False,
        'synthetic_dsdt_sha256':sha(d),'synthetic_ssdt_sha256':sha(s),
        'qemu_version':hw.get('qemu_version'),'modes':observations}
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if not check_results(result):
        raise SystemExit('S817 FAIL: inconsistent ACPICA replay evidence')
    print('STYLEOS S817 ACPICA REPLAY PASS (simulation only; no DMA authorization)')
if __name__=='__main__':main()
