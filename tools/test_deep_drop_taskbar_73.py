"""73 ordinary ISO: actual taskbar drag, cancellation, restore and private bytes."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

from test_deep_drop_guest import (Run, digest, provision_clients, verify_trace,
    CALL, CALL_KEYS, OFFER, ACCEPT, READ, FINISH, IDENTITY, CONTROLLER, DEVICE,
    firmware_path, records, complete_lines, Desktop)

GOLD = 'e6ad63'
LEFT, RIGHT = (105,105), (310,105)
SELECTED = 'StyleOS / Чай '.encode()

def exercise(r):
    source, target = r.launch(), r.launch()
    assert all(source[k] != target[k] for k in IDENTITY)
    initial = r.d.screen('73-open')
    # Initial ordinary shell opens exactly one legacy Files window; two real
    # native ports occupy the following slots, in admission order.
    step = min(160, (initial.w-252)//5)
    x,y,w,h = 164+2*step,initial.h-52,step-4,48
    task = (x+w//2,y+h//2)
    r.mark()
    r.d.click(*task)
    r.line(b'STYLEOS DD73 TASK ACTIVATE')
    r.mark()
    r.d.click(*task)
    r.line(b'STYLEOS DD73 TASK MINIMIZE')
    hidden = r.d.screen('73-minimized')
    assert hidden.pixels != initial.pixels
    assert hidden.pixels[3*(hidden.w*81+281):3*(hidden.w*81+281)+3] != bytes.fromhex('976332')
    r.private_text(source,SELECTED,target,b'')
    r.mark()
    time.sleep(.5)
    # A minimized authentic parked client must not be dispatched merely
    # because its user-mode read would be denied.
    assert not [v for _,v in records(r.lines()[r.point:],
        b'STYLEOS TEA70 WAKE',('pid','gen')) if v['pid']==target['pid']]
    def start_hover(name):
        r.d.move(*LEFT)
        r.mark()
        r.d.button(True)
        r.d.move(*task)
        frame = r.d.screen(name)
        assert frame.contains_color(x,y,w,h,bytes.fromhex(GOLD)), 'no destination outline'
        assert not records(r.lines()[r.point:],CALL,CALL_KEYS), 'published before release'
        return frame
    def no_transfer():
        assert b'STYLEOS DD73 TASK-GESTURE' not in r.lines()[r.point:]
        assert not [v for _,v in records(r.lines()[r.point:],CALL,CALL_KEYS)
                    if v['op']==OFFER and v['status']==0]
        r.private_text(source,SELECTED,target,b'')
    hovered = start_hover('73-hover-escape')
    r.d.key('esc')
    cleared = r.d.screen('73-escape-cleared')
    assert not cleared.contains_color(x,y,w,h,bytes.fromhex(GOLD))
    hovered.expect_changes_in(cleared,[(x,y,w,h)])
    r.d.button(False)
    no_transfer()
    start_hover('73-hover-refresh')
    r.d.key('f10')
    r.line(b'STYLEOS DESKTOP SHELL RESTART')
    # Move while still held, then release: must not become a fresh press.
    r.d.move(task[0]+2,task[1])
    r.d.button(False)
    no_transfer()
    start_hover('73-hover-success')
    r.d.button(False)
    r.line(b'STYLEOS DD73 TASK-GESTURE')
    token = r.call(source,OFFER,0)['result']
    assert token
    r.call(source,ACCEPT,2,id=token)
    r.receipt(b'STYLEOS TEA70 INPUT-COUNTS',
              ('pid','gen','empty','delivered','other','kind','code'),
              pid=target['pid'],gen=target['gen'],kind=4,code=2)
    r.park(target)
    r.private_text(source,SELECTED,target,b'')
    # Restored destination is the focused window: keyboard consent without
    # a click in its client area. The broker must deliver only once.
    r.mark()
    r.d.key('ret')
    r.call(target,ACCEPT,0,id=token)
    r.call(target,READ,0,id=token,result=len(SELECTED))
    r.call(target,FINISH,0,id=token)
    r.call(target,READ,4,id=token)
    r.frame(target,SELECTED,(281,81),'73-restored-document')
    r.private_text(source,SELECTED,target,SELECTED)
    for pos,app in ((LEFT,source),(RIGHT,target)):
        r.key(pos,'q')
        r.receipt(b'STYLEOS TEA69 RETIRED',('pid','window'),
                  pid=app['pid'],window=app['window'])
    r.receipt(b'STYLEOS TEA70 RESOURCES',
              ('phase','pid','resident','pages','owners','windows','ports'),
              phase=2,pid=target['pid'],resident=0,pages=0,owners=0,windows=0,ports=0)
    return token

def run(build,out,scenario=None,expected_gestures=1,expected_finishes=1):
    build,out=build.resolve(),out.resolve()
    out.mkdir(parents=True,exist_ok=False)
    iso=build/'StyleOS-owned-shell-865.iso'
    disk=out/'Deep-Drop-clients.raw'
    provision_clients(build,disk)
    before=digest(disk)
    vars_path=out/'OVMF_VARS.fd'
    shutil.copyfile(firmware_path('OVMF_VARS','OVMF_VARS'),vars_path)
    qemu=shutil.which('qemu-system-x86_64')
    version=subprocess.check_output([qemu,'--version'],text=True).splitlines()[0]
    assert version.startswith('QEMU emulator version 8.2.2 ')
    serial,trace=out/'serial.log',out/'cpu-trace.log'
    cmd=[qemu,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M',
         '-rtc','base=2026-11-25T12:34:00,clock=vm','-display','none','-vga','std',
         '-serial',f'file:{serial}','-monitor','none','-qmp','stdio','-net','none','-no-reboot','-S',
         '-drive',f'if=pflash,format=raw,readonly=on,file={firmware_path("OVMF_CODE","OVMF_CODE")}',
         '-drive',f'if=pflash,format=raw,file={vars_path}','-boot','order=d,strict=on','-cdrom',str(iso),
         '-device',f'piix3-ide,id={CONTROLLER},bus=pcie.0,addr=0x5',
         '-drive',f'if=none,id=data,format=raw,snapshot=on,file={disk}',
         '-device',f'ide-hd,id={DEVICE},bus={CONTROLLER}.1,unit=0,drive=data,serial=STYLEOS-S864-DATA',
         '-d','int,guest_errors','-D',str(trace)]
    with (out/'qemu.log').open('wb') as err, subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err) as proc:
        try:
            desktop=Desktop(proc,serial,out)
            assert 'QMP' in desktop.receive()
            desktop.call('qmp_capabilities')
            desktop.call('cont')
            r=Run(desktop,serial,out)
            r.line(b'STYLEOS DESKTOP READY')
            frame=desktop.screen('73-boot')
            desktop.x,desktop.y=frame.w//2,frame.h*3//4
            token=exercise(r) if scenario is None else scenario(r)
            desktop.call('stop')
            stats=next(item['stats'] for item in desktop.call('query-blockstats')
                       if item.get('qdev','').endswith(DEVICE) or item.get('device')=='data')
            assert stats['wr_operations']==0 and stats['wr_bytes']==0
            desktop.call('quit')
            assert proc.wait(timeout=15)==0
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=15)
    lines=complete_lines(serial.read_bytes())
    calls=records(lines,CALL,CALL_KEYS)
    faults=[] if scenario is None else token['faults']
    count=verify_trace(trace,calls,faults)
    assert lines.count(b'STYLEOS DD73 TASK-GESTURE')==expected_gestures
    assert len([v for _,v in calls if v['op']==FINISH and v['status']==0])==expected_finishes
    assert digest(disk)==before
    result=dict(status='PASS',qemu=version,iso_sha256=digest(iso),transfer=token,
                verified_cpl3_calls=count,prepublication_cancels=2,
                insertions=1,minimized_idle_observation_ms=500,guest_disk_writes=0,
                residual_leased_app_pages=0,private_memory=r.memory_checks,
                uart_sha256=digest(serial),trace_sha256=digest(trace))
    if scenario is not None:
        result.pop('transfer')
        result.pop('prepublication_cancels')
        result.pop('minimized_idle_observation_ms')
        result.update(token)
        result['insertions']=expected_finishes
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    run(args.build,args.output)
