"""Reject S821 forged output without trusting a printed PASS alone.

The real logs come from the same CI QEMU run and are not substituted by fixture.
"""
import argparse
from pathlib import Path
from acpica_core_821_verify import verify
MODES=('disabled','intel','bridge-intel','bridge-two-hop-intel','amd')

def check(folder:Path):
    total=0
    for mode in MODES:
        f=folder/mode/'serial.log'
        if not f.is_file():
            if mode=='amd':continue
            raise RuntimeError('missing genuine QEMU log '+mode)
        raw=f.read_bytes()
        assert verify(raw,33,mode)[0], mode
        marker=b'STYLEOS S821 POLICY upstream-real-code=yes'
        a=raw.index(marker)
        variants=[raw.replace(b'STYLEOS S821 PASS',b'STYLEOS S821 FAIL'),
                  raw.replace(marker,b'STYLEOS S821 POLICY upstream-real-code=no'),
                  raw.replace(b'aml-executed=no',b'aml-executed=yes'),
                  raw.replace(b'dma-grants=no',b'dma-grants=yes'),
                  raw.replace(b'production-cr3=deny',b'production-cr3=allow'),
                  raw.replace(b'known-good=yes',b'known-good=no'),
                  raw.replace(b'known-bad-rejected=yes',b'known-bad-rejected=no'),
                  raw.replace(b'firmware-copy=yes',b'firmware-copy=no'),
                  raw.replace(b'full-core-compiled=yes',b'full-core-compiled=no'),
                  raw.replace(b'STYLEOS S821 PASS\r\n',b''),
                  raw.replace(b'STYLEOS S821 PASS',b'STYLEOS S821 PASS EXTRA'),
                  raw[:a]+raw[a:].replace(b'dma-grants=no',b'dma-grants=yes',1),
                  raw.replace(b'0x000000000000',b'0x000000ffffffffff',1),
                  raw.replace(b'STYLEOS S821 ACPICA-CHECKSUM',b'STYLEOS S821 ACPICA-CHECKSUM BAD',1),
                  raw+raw[raw.index(b'STYLEOS S821 PASS'):raw.index(b'STYLEOS S821 PASS')+len(b'STYLEOS S821 PASS')]+b'\n']
        for v in variants:
            assert not verify(v,33,mode)[0], (mode,total)
            total+=1
        assert not verify(raw,0,mode)[0];total+=1
    assert total>=64
    print('STYLEOS S821 NEGATIVE PASS',str(total)+'/'+str(total))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--qemu-dir',type=Path,required=True)
    check(ap.parse_args().qemu_dir)
