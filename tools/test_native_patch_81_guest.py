"""Actual CPL3 patch wire, generation, partial compositor and cleanup."""
import argparse
from pathlib import Path
import test_native_sdk_77_guest as sdk
import test_native_expanded_76_guest as old

OPS=(0x103,0x121,0x120,0x120,0x120,0x120,0x120,0x121)
CALL=b'STYLEOS NATIVE PATCH-CALL'
KEYS=('pid','gen','root','op','window','status','generation')

def prepare(out):
    return sdk.prepare(out,client='native_patch_81_client.c',target_name='ipc81')

def before_release(desktop,r,apps):
    a=apps[0]
    r.receipt(CALL,KEYS,pid=a['pid'],op=0x121,status=0,generation=3)
    # Wait for actual compositor paint rather than equating UART with pixels.
    def visible(_):
        try:
            shot=desktop.screen('patch81-visible')
            shot.check(113,113,'91c725')
            shot.check(127,127,'91c725')
            shot.check(111,113,'224466')
            shot.check(128,113,'224466')
            return True
        except (AssertionError,ValueError):
            return False
    r.until(visible,'actual patch and unchanged neighbor pixels')

def verify(trace,lines,apps,pcs):
    result=old.verify(trace,lines,apps,pcs,sdk_stack=True,extra_ops=OPS)
    a=apps[0]
    calls=list(old.records(lines,CALL,KEYS))
    expected=[(0x121,0,2),(0x120,1,0),(0x120,0,3),(0x120,7,0),
              (0x120,0,3),(0x120,1,0),(0x121,0,3)]
    assert len(calls)==len(expected)
    for (_,c),(op,status,gen) in zip(calls,expected):
        assert c==dict(pid=a['pid'],gen=a['gen'],root=a['root'],op=op,window=0,status=status,generation=gen)
    check=list(old.records(lines,b'STYLEOS PATCH81 VERIFIED',('pid','window','generation')))
    assert len(check)==1 and check[0][1]['pid']==a['pid'] and check[0][1]['generation']==3
    assert check[0][0]>calls[-1][0]
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',required=True,type=Path)
    old.run(p.parse_args().output,prepare_case=prepare,verify_case=verify,before_release=before_release)
