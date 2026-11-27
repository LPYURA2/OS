"""Extra ordinary ISO UI checks, entirely on host. No kernel test module."""
from uart_receipts import complete_lines
import hashlib,json
from test_desktop_v0 import Frame
from runtime_mixed_863_verify import row

EVENT=b'STYLEOS S863 EVENT '
KEYS=('root','n','batch','mouse','key','dx','dy','buttons','modifiers','x','y','active','ru','restarts','text','text-hash','text-dirty','note-session','writes')
# Event schema also includes trailing input flags in some profiles: parse
# by key, requiring exact values for the properties being checked.
def events(raw):
    return [{k.decode():int(v,16) for k,v in (p.split(b'=',1) for p in x.split()[3:])}
            for x in raw if x.startswith(EVENT)]

def exercise(d,proc,serial,folder,wait):
    def raw():return complete_lines(serial.read_bytes())
    def key(name,code):
        before=len(events(raw()))
        d.key(name);wait(proc,serial,EVENT,before+1)
        receipt=events(raw())[-1]
        if receipt['key']!=code or receipt['mouse']:raise ValueError('extended input '+name)
        if code in (0x3e,0x42):
            count=sum(x==b'STYLEOS SHELL ACTION RENDERED' for x in raw())
            # The receipt may already have been followed by the render.
            expected=sum(e['key'] in (0x3e,0x42) and not e['mouse'] for e in events(raw()))
            if count<expected:wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',expected)
        return receipt
    def move(x,y):
        before=len(events(raw()))
        d.move(x,y);wait(proc,serial,EVENT,before+1)
        result=events(raw())[-1]
        if (result['x'],result['y'])!=(x,y) or not result['mouse']:raise ValueError('cursor location')
        return result
    move(25,420)
    initial=d.screen('extended-cursor-start')
    mv=move(50,435)
    shifted=d.screen('extended-cursor-end')
    initial.expect_changes_in(shifted,[(25,420,10,17),(50,435,10,17)])
    if not 0<mv['writes']<=340:raise ValueError('cursor damage budget')
    opened=key('f4',0x3e); menu=d.screen('extended-menu')
    closed=key('f4',0x3e); after=d.screen('extended-menu-closed')
    if menu.pixels==shifted.pixels or menu.pixels==after.pixels:raise ValueError('menu not rendered')
    menu.check(37,menu.h-54-273+19,'76572f')
    procs=key('f8',0x42); pf=d.screen('extended-processes')
    if pf.pixels==after.pixels:raise ValueError('process window not drawn')
    px,py=pf.w//3,(pf.h-54)//6
    pf.check(px+12,py,'e6ad63')
    from native_process_view_02 import verify_snapshot
    verify_snapshot(raw(),live=False)
    # F10 is a shell refresh, not a kernel reboot. Track the actual note
    # session and text receipts through the refresh and subsequent input.
    before=procs
    restart=key('f10',0x44)
    wait(proc,serial,b'STYLEOS DESKTOP SHELL RESTART',1)
    lang=key('f6',0x40)
    if restart['restarts']!=before['restarts'] or lang['restarts']!=before['restarts']+1 or any(restart[k]!=before[k] or lang[k]!=before[k] for k in ('text','text-hash','text-dirty','note-session')):raise ValueError('deferred shell restart state')
    rf=d.screen('extended-restart')
    restored=key('f6',0x40)
    if lang['ru']==restart['ru'] or restored['ru']!=restart['ru']:raise ValueError('language switching')
    (folder/'extended-desktop.json').write_text(json.dumps({'cursor':mv,'menu':opened,'closed':closed,'processes':procs,'restart':restart,'language':lang,'restored':restored,'frames':{x:hashlib.sha256((folder/(x+'.ppm')).read_bytes()).hexdigest() for x in ('extended-cursor-start','extended-cursor-end','extended-menu','extended-menu-closed','extended-processes','extended-restart')}},indent=2)+'\n')

def verify(folder,lines,root):
    from native_process_view_02 import verify_snapshot
    verify_snapshot(lines,live=False)
    data=json.loads((folder/'extended-desktop.json').read_text())
    actual=events(lines)
    for name in ('cursor','menu','closed','processes','restart','language','restored'):
        receipt=data[name]
        if receipt['root']!=root or receipt not in actual:raise ValueError('extended receipt '+name)
    if lines.count(b'STYLEOS DESKTOP SHELL RESTART')!=1:raise ValueError('shell refresh marker')
    from native_forced_stop_verify import records
    admitted=records(lines,b'STYLEOS NATIVE SHELL-CONTROLLER',('pid','gen','root','epoch'))
    calls=records(lines,b'STYLEOS NATIVE SHELL-CALL',('pid','gen','root','op','reply'))
    applied=records(lines,b'STYLEOS NATIVE SHELL-APPLY',('epoch',))
    retired=records(lines,b'STYLEOS NATIVE SHELL-CONTROLLER-RETIRED',('pid','epoch','resident'))
    actions=records(lines,b'STYLEOS NATIVE SHELL-ACTION',('epoch','action'))
    submits=records(lines,b'STYLEOS NATIVE SHELL-SUBMIT',('key',))
    renders=[i for i,x in enumerate(lines) if x==b'STYLEOS SHELL ACTION RENDERED']
    if (len(admitted),len(calls),len(applied),len(retired),len(actions),len(submits),len(renders))!=(4,8,1,4,3,4,3):
        raise ValueError('ordinary controller lifecycle count')
    keys=(0x3e,0x3e,0x42,0x44)
    if [x['key'] for _,x in submits]!=list(keys) or [tuple(x.values()) for _,x in actions]!=[(1,2),(2,2),(3,3)]:
        raise ValueError('ordinary command policy')
    for j,(i,a) in enumerate(admitted):
        pid,gen,child,epoch=a.values()
        if (pid,gen,epoch)!=(j+1,j+1,j+1) or child==root or not child:raise ValueError('ordinary controller identity')
        if [tuple(x.values()) for _,x in calls[2*j:2*j+2]]!=[(pid,gen,child,0x105,(epoch<<32)|keys[j]),(pid,gen,child,0x106,epoch)] or tuple(retired[j][1].values())!=(pid,epoch,0):
            raise ValueError('ordinary controller syscall and cleanup')
        if not submits[j][0]<i<calls[2*j][0]<calls[2*j+1][0]:raise ValueError('ordinary controller order')
        if j<3 and not calls[2*j+1][0]<retired[j][0]<actions[j][0]<renders[j]<submits[j+1][0]:
            raise ValueError('command after retirement and actual render')
    if applied[0][1]['epoch']!=4 or not calls[-1][0]<applied[0][0]<lines.index(b'STYLEOS DESKTOP SHELL RESTART'):
        raise ValueError('ordinary controller refresh ordering')
    if lines.count(b'STYLEOS SHELL PROVIDER READY')!=1:raise ValueError('ordinary provider readiness')
    if not any(x==b'STYLEOS DESKTOP CURSOR DAMAGE PASS' for x in lines):raise ValueError('cursor proof missing')
    for name,expected in data['frames'].items():
        if hashlib.sha256((folder/(name+'.ppm')).read_bytes()).hexdigest()!=expected:raise ValueError('UI frame digest')
