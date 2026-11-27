"""74: genuine ordinary taskbar revoke, busy refusal, fault and fresh transfer."""
import argparse
from pathlib import Path
from test_deep_drop_taskbar_73 import run, LEFT, RIGHT, SELECTED, GOLD
from test_deep_drop_guest import CALL, CALL_KEYS, OFFER, ACCEPT, READ, FINISH, records

def exercise(r):
    source,target=r.launch(),r.launch()
    initial=r.d.screen('74-open')
    step=min(160,(initial.w-252)//5)
    x,y,w,h=164+2*step,initial.h-52,step-4,48
    task=(x+w//2,y+h//2)
    def click_task(marker):
        r.mark()
        r.d.click(*task)
        r.line(marker)
    def hover(name):
        r.d.move(*LEFT)
        r.mark()
        r.d.button(True)
        r.d.move(*task)
        assert r.d.screen(name).contains_color(x,y,w,h,bytes.fromhex(GOLD))
        r.d.button(False)
    def offer(name):
        hover(name)
        r.line(b'STYLEOS DD73 TASK-GESTURE')
        token=r.call(source,OFFER,0)['result']
        assert token
        r.call(source,ACCEPT,2,id=token)
        r.receipt(b'STYLEOS TEA70 INPUT-COUNTS',
            ('pid','gen','empty','delivered','other','kind','code'),
            pid=target['pid'],gen=target['gen'],kind=4,code=2)
        r.park(target)
        return token
    click_task(b'STYLEOS DD73 TASK ACTIVATE')
    click_task(b'STYLEOS DD73 TASK MINIMIZE')
    first=offer('74-first-offer')
    r.private_text(source,SELECTED,target,b'')
    # The restored target is focused. A second click minimizes it, revoking
    # the pending offer. Restoring the same window cannot resurrect old ID.
    click_task(b'STYLEOS DD73 TASK MINIMIZE')
    r.line(b'STYLEOS DD72 REVOKED')
    click_task(b'STYLEOS DD73 TASK ACTIVATE')
    r.mark()
    r.d.key('ret')
    r.call(target,ACCEPT,4,id=first)
    r.park(target)
    r.private_text(source,SELECTED,target,b'')
    click_task(b'STYLEOS DD73 TASK MINIMIZE')
    second=offer('74-second-offer')
    assert second>first
    # Busy broker refuses another gesture even with valid source and target.
    hover('74-busy-hover')
    assert b'STYLEOS DD73 TASK-GESTURE' not in r.lines()[r.point:]
    assert not [v for _,v in records(r.lines()[r.point:],CALL,CALL_KEYS) if v['op']==OFFER]
    r.private_text(source,SELECTED,target,b'')
    # A real CPL3 fault in the destination revokes the offered transfer.
    r.key(RIGHT,'x')
    r.line(b'STYLEOS DD72 REVOKED')
    r.receipt(b'STYLEOS TEA69 RETIRED',('pid','window'),
              pid=target['pid'],window=target['window'])
    r.receipt(b'STYLEOS TEA70 RESOURCES',
              ('phase','pid','resident','pages','owners','windows','ports'),
              phase=2,pid=target['pid'],resident=1,pages=3,owners=1,windows=1,ports=1)
    fault_root=target['root']
    old=target
    target=r.launch()
    assert all(target[k]!=old[k] for k in ('pid','gen','window'))
    r.private_text(source,SELECTED,target,b'')
    click_task(b'STYLEOS DD73 TASK ACTIVATE')
    click_task(b'STYLEOS DD73 TASK MINIMIZE')
    third=offer('74-fresh-target')
    assert third>second
    r.mark()
    r.d.key('ret')
    r.call(target,ACCEPT,0,id=third)
    r.call(target,READ,0,id=third,result=len(SELECTED))
    r.call(target,FINISH,0,id=third)
    r.call(target,READ,4,id=third)
    r.frame(target,SELECTED,(281,81),'74-recovered')
    r.private_text(source,SELECTED,target,SELECTED)
    for pos,app in ((LEFT,source),(RIGHT,target)):
        r.key(pos,'q')
        r.receipt(b'STYLEOS TEA69 RETIRED',('pid','window'),pid=app['pid'],window=app['window'])
    r.receipt(b'STYLEOS TEA70 RESOURCES',
              ('phase','pid','resident','pages','owners','windows','ports'),
              phase=2,pid=target['pid'],resident=0,pages=0,owners=0,windows=0,ports=0)
    return dict(offers=[first,second,third],visibility_revocations=1,
                busy_refusals=1,endpoint_faults=1,faults=[fault_root])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    run(args.build,args.output,scenario=exercise,expected_gestures=3,expected_finishes=1)
