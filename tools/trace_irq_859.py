#!/usr/bin/env python3
"""Replay the exact failed S859 CI image, with QEMU CPU event tracing.
Never interpret a diagnostic replay as kernel acceptance.
"""
import argparse,gzip,hashlib,json,re,shutil,subprocess
from pathlib import Path
EXPECTED={
 'serial.log':'ae56733e7f64bba3c69d14c34afad04a49a23f6f54cbef932bc726af82acfde9',
 'build/x86_64/BOOTX64.EFI':'bf7a411d9ef84bb49508b0bae81e9d21e912cf398d0b2e1fb9e5d41a86bd7b94',
 'build/x86_64/kernel.map':'431f8f63e72c69e9a63fe6bf3e7fe335a25f47e5976e2cc64718401896ae9c79',
 'build/x86_64/styleos.img':'da8148c78ed8a60f57a9c73567baa4e5b62e4396d21a85f8d2a9e6ad8b379bdd',
}
def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
HEADER=re.compile(r'^\s*(\d+): v=([0-9a-fA-F]{2}) e=([0-9a-fA-F]+) i=([01]) cpl=([0-3]) IP=([0-9a-fA-F]+):([0-9a-fA-F]+)\b',re.M)
def events(trace,root):
    matches=list(HEADER.finditer(trace));out=[]
    for i,m in enumerate(matches):
        block=trace[m.start():matches[i+1].start() if i+1<len(matches) else len(trace)]
        r=re.search(r'\bCR3=([0-9a-fA-F]+)\b',block)
        if r is None or int(r[1],16)!=root:continue
        g=m.groups();cr2=re.search(r'\bCR2=([0-9a-fA-F]+)\b',block)
        out.append({'sequence':int(g[0]),'vector':int(g[1],16),'raw_error_field':int(g[2],16),'software':bool(int(g[3])),'cpl':int(g[4]),'cs':int(g[5],16),'rip':int(g[6],16),'cr3':root,'cr2_raw':int(cr2[1],16) if cr2 else None})
    return out

def active_root(raw):
    m=re.findall(rb'^STYLEOS S851 ACTIVE root=0x([0-9a-f]{16})\b',raw,re.M)
    if len(m)!=1:raise ValueError('missing or ambiguous owned root')
    return int(m[0],16)

def main():
    p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--code',type=Path,required=True);p.add_argument('--vars',type=Path,required=True);a=p.parse_args()
    original=a.original.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    for name,expected in EXPECTED.items():
        if digest(original/name)!=expected:raise ValueError('original hash mismatch: '+name)
    old=json.loads((original/'results.json').read_text())
    if old['status']!='FAIL' or old['qemu_exit_code']!=69:raise ValueError('wrong original failure')
    q=shutil.which('qemu-system-x86_64')
    if not q:raise RuntimeError('QEMU required')
    vars=out/'OVMF_VARS.fd';shutil.copyfile(a.vars,vars)
    trace=out/'cpu-trace.log';serial=out/'serial.log'
    cmd=[q,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M','-display','none','-vga','std','-serial',f'file:{serial}','-monitor','none','-net','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-drive',f'if=pflash,format=raw,readonly=on,file={a.code.resolve()}','-drive',f'if=pflash,format=raw,file={vars}','-drive',f'if=none,id=styleos,format=raw,readonly=on,file={original/"build/x86_64/styleos.img"}','-device','virtio-blk-pci,drive=styleos','-d','int,guest_errors','-D',str(trace)]
    provenance={'qemu_version':subprocess.check_output([q,'--version'],text=True),'command':cmd,'original_sha256':EXPECTED,'ovmf_code_sha256':digest(a.code),'ovmf_fresh_vars_sha256':digest(a.vars)}
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    rc=None
    try:
        with (out/'qemu.log').open('wb') as log:rc=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=100).returncode
    except subprocess.TimeoutExpired:pass
    raw=serial.read_bytes() if serial.exists() else b''
    text=trace.read_text(errors='replace') if trace.exists() else ''
    root=None;observed=[]
    try:root=active_root(raw);observed=events(text,root)
    except ValueError:pass
    report={'diagnostic_only':True,'kernel_accepted':False,'exit':rc,'owned_root':root,'owned_root_events':observed,'original_failure_reproduced':rc==69 and b'STYLEOS S859 LVT-MASKED ' in raw and b'STYLEOS S859 LVT-RESTORED ' not in raw,'trace_sha256':digest(trace) if trace.exists() else None,'serial_sha256':digest(serial) if serial.exists() else None}
    (out/'observations.json').write_text(json.dumps(report,indent=2)+'\n')
    (out/'trace-tail.txt').write_text('\n'.join(text.splitlines()[-220:])+'\n')
    if trace.exists():
        with trace.open('rb') as f,gzip.open(out/'cpu-trace.log.gz','wb') as g:shutil.copyfileobj(f,g)
        trace.unlink()
    print(json.dumps(report,indent=2),flush=True)
    if not(report['original_failure_reproduced'] and observed):raise SystemExit('diagnostic replay inconclusive; preserve raw capture')
if __name__=='__main__':main()
