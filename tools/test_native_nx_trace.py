"""CR2 parser contract, modeled on actual QEMU 8.2.2 output."""
import unittest
from native_catalog_865_verify import verify_nx_cr2

BLOCK = """   148: v=0e e=0015 i=0 cpl=3 IP=005b:0000000011000000 pc=0000000011000000 SP=0063:0000000011001000 CR2=0000000011000000
CR0=80010033 CR2=0000000011000000 CR3=0000000005b09000 CR4=00000668
"""
class NXTraceTest(unittest.TestCase):
    def test_agreement(self):
        verify_nx_cr2(BLOCK,0x11000000)
    def test_refusals(self):
        value='CR2=0000000011000000'
        for changed in (BLOCK.replace(value,'CR2=0000000011000001',1),
                        BLOCK.replace(value,'',1),BLOCK+value,
                        BLOCK.replace('v=0e','v=0d'),BLOCK.replace('CR0=','BAD=')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                verify_nx_cr2(changed,0x11000000)
if __name__=='__main__':unittest.main()
