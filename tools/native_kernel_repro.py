#!/usr/bin/env python3
"""KFA-01: compare independent ordinary builds, preserve actual tool identity."""
import argparse, hashlib, json, os, platform, subprocess, sys
from pathlib import Path
from native_kernel_profile import SOURCES

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for x in ('first','second','output'):p.add_argument('--'+x,type=Path,required=True)
    p.add_argument('--sha',required=True)
    a=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    if head!=a.sha or a.first.resolve()==a.second.resolve():raise ValueError('wrong source or same build')
    tree=subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=root,text=True).strip()
    manifests=[json.loads((x/'owned-shell-compiler.json').read_text()) for x in (a.first,a.second)]
    for m in manifests:
        if m['sources']!=list(SOURCES) or m['profile']!='owned-shell-candidate' or m['test_sources'] or any(x.startswith('-DSTYLEOS_TEST_') or x.startswith('-DSTYLEOS_NATIVE_PINNED_CATALOG') for x in m['flags']):
            raise ValueError('not clean ordinary profile')
        if m['source_sha256']!={s:digest(root/s) for s in SOURCES}:raise ValueError('source mismatch')
    if manifests[0]!=manifests[1]:raise ValueError('compiler manifest differs')
    names=['BOOTX64.EFI','styleos.img','StyleOS-owned-shell-865.iso']
    hashes={name:[digest(x/name) for x in (a.first,a.second)] for name in names}
    tools={}
    for name,cmd in [('clang',[os.environ.get('CLANG','clang'),'--version']),('lld',[os.environ.get('LLD_LINK','lld-link'),'--version']),('xorriso',['xorriso','-version']),('mtools',['mcopy','-V']),('mkfs.fat',['mkfs.fat','--help']),('qemu',['qemu-system-x86_64','--version'])]:
        result=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20)
        if not result.stdout.strip():raise ValueError('missing tool identity '+name)
        tools[name]=result.stdout.strip()
    match=all(x[0]==x[1] for x in hashes.values())
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'status':'PASS' if match else 'FAIL','sha':head,'tree':tree,'platform':platform.platform(),'python':sys.version,'tools':tools,'compiler':manifests[0],'sha256':hashes},indent=2)+'\n')
    if not match:raise ValueError('ordinary EFI/IMG/ISO not reproducible')
    print('KFA-01 independent ordinary EFI/IMG/ISO byte reproducibility PASS')
if __name__=='__main__':main()
