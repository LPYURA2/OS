"""Verify exact generated package identity, executable stores and reproducibility."""
import argparse,hashlib,struct,zipfile
from pathlib import Path
from build_native_sample_69 import build
from tea_package import inspect
def run(folder):
    folder.mkdir(parents=True,exist_ok=True)
    build(folder)
    fingerprints=[]
    packages=[]
    for i,(name,color) in enumerate((('Native-sample',0x2579a1),('Native-peer',0x976332))):
        path=folder/(name+'.tea')
        manifest=inspect(path)
        with zipfile.ZipFile(path) as z:elf=z.read('bin/x86_64/app.elf')
        assert manifest['id']=='styleos.native_sample_'+str(i)
        assert manifest['name']==name and manifest['permissions']==[]
        assert elf==(folder/(name+'.elf')).read_bytes()
        assert hashlib.sha256(elf).hexdigest()==manifest['sha256']
        assert elf[4096:4101]==b'\xbe\x00\x00\x00\x12'
        for j,value in enumerate((0,0,160,120,color)):
            offset=4101+7*j
            assert elf[offset:offset+3]==b'\xc7\x46'+bytes([4*j])
            assert struct.unpack_from('<I',elf,offset+3)[0]==value
        fingerprints.append(manifest['sha256'])
        packages.append(path.read_bytes())
    assert len(set(fingerprints))==2
    assert {p.name for p in folder.glob('*.tea')}=={'Native-sample.tea','Native-peer.tea'}
    header=(folder/'native_sample_69.h').read_bytes()
    build(folder)
    assert (folder/'native_sample_69.h').read_bytes()==header
    for name,expected in zip(('Native-sample','Native-peer'),packages):
        assert (folder/(name+'.tea')).read_bytes()==expected
    print('TEA69 distinct payloads, exact names, colors and reproducibility PASS')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
