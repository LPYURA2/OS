from pathlib import Path
import sys
sys.path.insert(0, '/workspace/OS')
from tools.test import serial_pool_ok

def row(kind, a, b):
    return f'STYLEOS MEMMAP {kind} 0x{a:016x} 0x{b:016x}\n'.encode()
prefix=(b'STYLEOS x86_64 ExitBootServices OK\nSTYLEOS HANDOFF PASS x86_64\n'
        b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
for kind,a,b in [('IMAGE',0x100000,0x130000),('STACK',0x100000,0x110000),
                 ('HANDOFF',0x110000,0x110060),('SOURCE',0x200000,0x201000),
                 ('RAM',0x1000,0x400000),('RESERVED',0x100000,0x130000),
                 ('RESERVED',0x200000,0x201000),('CANDIDATE',0x1000,0x100000),
                 ('CANDIDATE',0x130000,0x200000),('CANDIDATE',0x201000,0x400000)]:
    prefix+=row(kind,a,b)
prefix+=b'STYLEOS MEMMAP PASS\n'
tail=(b'STYLEOS BOOTPOOL RANGE 0x0000000000111000 0x0000000000119000\n'
      b'STYLEOS BOOTPOOL META 0x0000000000119000 0x000000000011a000\n'
      b'STYLEOS BOOTPOOL WORKSPACE 0x000000000011a000 0x000000000011d078\n'
      b'STYLEOS BOOTPOOL ADMIT PASS\n')
for i in range(8):
    tail+=f'STYLEOS BOOTPOOL ALLOC {i} 0x{0x111000+i*4096:016x} ZERO\n'.encode()
tail+=(b'STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096\n'
       b'STYLEOS BOOTPOOL EXHAUSTED PASS\n'
       b'STYLEOS BOOTPOOL REUSE 0x0000000000114000 ZERO\n'
       b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n')
good=prefix+tail
site=dict(arch='x86_64',address_space='image-rva',pool_start=0x11000,pool_end=0x19000,
          metadata_start=0x19000,metadata_end=0x1a000,pool_bytes=32768,
          metadata_bytes=4096,page_size=4096,image_start=0,image_end=0x30000,
          image_size=0x30000,pool_rva_start=0x11000,pool_rva_end=0x19000,
          metadata_rva_start=0x19000,metadata_rva_end=0x1a000,preferred_image_base=0x140000000)
assert serial_pool_ok(good,'x86_64',site)
print('valid relocated x86 fixture: PASS')
mutations={
 'empty':b'',
 'PASS-only':b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n',
 'missing pool end':good.replace(b'0x0000000000119000\n',b'\n',1),
 'pool end one byte short':good.replace(b'0x0000000000119000\n',b'0x0000000000118fff\n',1),
 'workspace overlap':good.replace(b'WORKSPACE 0x000000000011a000',b'WORKSPACE 0x0000000000118000'),
 'metadata overlap':good.replace(b'META 0x0000000000119000 0x000000000011a000',b'META 0x0000000000118000 0x0000000000119000'),
 'duplicate issue':good.replace(b'ALLOC 1 0x0000000000112000',b'ALLOC 1 0x0000000000111000'),
 'missing RW':good.replace(b'STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096\n',b''),
 'error before':b'STYLEOS BOOTPOOL ERROR: range\n'+good,
 'error after':good+b'STYLEOS BOOTPOOL ERROR: range\n',
 'timeout before':b'QEMU timeout\n'+good,
 'timeout after':good+b'QEMU timed out\n',
 'preferred base used':good.replace(b'0x0000000000111000',b'0x0000000140011000'),
 'no image proof':prefix+b'STYLEOS BOOTPOOL PASS\nSTYLEOS KERNEL HALT\n',
}
for label,output in mutations.items():
    assert not serial_pool_ok(output,'x86_64',site),label
    print(label+': rejected PASS')
for label,changed in [('wrong-sidecar-end',dict(site,pool_end=site['pool_end']+4096)),
                       ('boolean-size',dict(site,page_size=True)),
                       ('missing-proof',{}),('wrong-arch',dict(site,arch='aarch64'))]:
    assert not serial_pool_ok(good,'x86_64',changed),label
    print(label+': rejected PASS')
print('Independent classifier probes: 1 valid + 18 negative PASS')
