#!/usr/bin/env python3
"""Audit original hardware UART records produced by two S796 QEMU tests in this CI."""
import argparse,hashlib,json
from pathlib import Path
from pci_forwarding_797 import ForwardingError,audit

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--default',required=True,type=Path)
    p.add_argument('--bridge',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    observations={}
    for name,folder,need in [('default',a.default,False),('bridge',a.bridge,True)]:
        uart=(folder/'serial.log').read_bytes()
        result=json.loads((folder/'results.json').read_text())
        if not (result.get('stage')==796 and result.get('status')=='PASS' and
                result.get('qemu_exit_code')==33 and
                result.get('production_cr3_authorized') is False and
                result.get('unknown_bar_sizes_remain') is True and
                result.get('scenario')==name and
                result.get('uart_sha256')==hashlib.sha256(uart).hexdigest()):
            raise ForwardingError('hardware origin/hash mismatch '+name)
        if need and result.get('bridge_enumeration_verified') is not True:
            raise ForwardingError('bridge not observed by QEMU')
        observations[name]={'serial_sha256':result['uart_sha256'],
                            **audit(uart,33,require_bridge=need)}
    if observations['default']['bridges']!=0 or observations['default']['buses']!=1:
        raise ForwardingError('default topology mismatch')
    if observations['bridge']['buses']<2 or observations['bridge']['routed_memory_bar_bases_in_bridge_windows']<2:
        raise ForwardingError('no independently routed MMIO BAR bases')
    out={'stage':797,'status':'PASS','source':'same-run-hardware-S796-QEMU',
         'verified_only':'BAR-base-in-bridge-window',
         'production_cr3_authorized':False,'observations':observations}
    (a.output/'results.json').write_text(json.dumps(out,indent=2,sort_keys=True)+'\n')
    print('STYLEOS S797 HARDWARE-BASED FORWARDING AUDIT PASS')
if __name__=='__main__':main()
