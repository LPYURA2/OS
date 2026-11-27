"""Extra ordinary ISO UI checks, entirely on host. No kernel test module."""
from uart_receipts import complete_lines
import hashlib,json,time
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
        if code in (0x3e,0x42,0x01):
            count=sum(x==b'STYLEOS SHELL ACTION RENDERED' for x in raw())
            # The receipt may already have been followed by the render.
            expected=sum(e['key'] in (0x3e,0x42,0x01) and not e['mouse'] for e in events(raw()))
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
    procs=key('ctrl-shift-esc',0x01); pf=d.screen('extended-processes')
    if pf.pixels==after.pixels:raise ValueError('process window not drawn')
    px,py=pf.w//3,(pf.h-54)//6
    pf.check(px+12,py,'e6ad63')
    from native_process_view_02 import verify_snapshot
    verify_snapshot(raw(),live=False)
    # Exercise real title controls on the processes window. The input path
    # submits a hit-region distance, not a preselected maximize/close action.
    def title(x,y):
        count=sum(t==b'STYLEOS SHELL ACTION RENDERED' for t in raw())
        d.click(x,y)
        wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',count+1)
    title(px+min(pf.w//2,510)-70,py+19)
    big=d.screen('extended-window-max')
    if big.pixels==pf.pixels:raise ValueError('maximize missing')
    title(pf.w-70,19)
    normal=d.screen('extended-window-restore')
    normal.check(px+12,py,'e6ad63')
    if normal.pixels==big.pixels:raise ValueError('restore missing')
    count=sum(t==b'STYLEOS SHELL ACTION RENDERED' for t in raw())
    d.move(px+80,py+19);d.button(True)
    wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',count+1)
    d.move(px+110,py+39);d.button(False)
    wait(proc,serial,b'STYLEOS DESKTOP WINDOW DRAG',1)
    moved=d.screen('extended-window-drag')
    moved.check(px+30+12,py+20,'e6ad63')
    if moved.pixels==normal.pixels:raise ValueError('drag missing')
    px+=30;py+=20
    title(px+min(pf.w//2,510)-110,py+19)
    minimized=d.screen('extended-window-min')
    if minimized.pixels==moved.pixels:raise ValueError('minimize missing')
    # FILES and (on read boot) NOTES precede TASKS in the panel order.
    task_index=1 if procs['note-session'] else 2
    title(164+task_index*160+40,pf.h-54+20)
    returned=d.screen('extended-window-taskbar')
    returned.check(px+12,py,'e6ad63')
    if returned.pixels==minimized.pixels:raise ValueError('taskbar restore missing')
    title(px+min(pf.w//2,510)-25,py+19)
    gone=d.screen('extended-window-close')
    if gone.pixels==returned.pixels:raise ValueError('close missing')
    title(30,pf.h-54+20)
    title(40,pf.h-54-273+75)
    about=d.screen('extended-about')
    ax,ay=pf.w//3,(pf.h-54)//5
    about.check(ax+12,ay,'e6ad63')
    count=sum(t==b'STYLEOS SHELL ACTION RENDERED' for t in raw())
    d.key('alt-f4');wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',count+1)
    about_closed=d.screen('extended-about-close')
    if about_closed.pixels==about.pixels:raise ValueError('about close missing')
    before_switch=move(50,435)
    count=sum(t==b'STYLEOS SHELL ACTION RENDERED' for t in raw())
    switched_key=key('alt-tab',0x0f)
    wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',count+1)
    after_switch=move(55,435)
    if switched_key['modifiers']!=4 or after_switch['active']==before_switch['active'] or after_switch['active'] not in (0,1,4):
        raise ValueError('Alt+Tab visible legacy focus')
    # F10 is a shell refresh, not a kernel reboot. Track the actual note
    # session and text receipts through the refresh and subsequent input.
    before=procs
    restart=key('f10',0x44)
    wait(proc,serial,b'STYLEOS DESKTOP SHELL RESTART',1)
    lang=key('alt-shift',0x200)
    if restart['restarts']!=before['restarts'] or lang['restarts']!=before['restarts']+1 or any(restart[k]!=before[k] or lang[k]!=before[k] for k in ('text','text-hash','text-dirty','note-session')):raise ValueError('deferred shell restart state')
    rf=d.screen('extended-restart')
    restored=key('shift-alt',0x200)
    if lang['modifiers']!=5 or restored['modifiers']!=5 or lang['ru']==restart['ru'] or restored['ru']!=restart['ru']:raise ValueError('language switching')
    # A real standalone pinned .tea runs on explicit F7, no fixture catalog.
    # Wait for authenticated frame publication, then focus and send Q.
    timings={}
    started=time.monotonic_ns()
    d.key('f7');wait(proc,serial,b'STYLEOS TEA69 LAUNCHED',1)
    wait(proc,serial,b'STYLEOS NATIVE FRAME',1)
    wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',14)
    # Frame publication precedes next desktop damage iteration. Move
    # into its interior and wait for the real input/render boundary.
    move(100,100)
    sample=d.screen('extended-tea-live')
    sample.check(81,81,'2579a1');sample.check(239,199,'2579a1')
    timings['first_launch_to_frame_ms']=(time.monotonic_ns()-started)/1e6
    started=time.monotonic_ns()
    d.key('f7');wait(proc,serial,b'STYLEOS TEA69 LAUNCHED',2)
    wait(proc,serial,b'STYLEOS NATIVE FRAME',2)
    wait(proc,serial,b'STYLEOS SHELL ACTION RENDERED',15)
    move(100,101)
    both=d.screen('extended-tea-both')
    both.check(81,81,'2579a1');both.check(281,81,'976332')
    timings['disk_launch_to_frame_ms']=(time.monotonic_ns()-started)/1e6
    # Repeat the ordinary F10 refresh while both native apps are resident.
    live_restart=key('f10',0x44)
    wait(proc,serial,b'STYLEOS DESKTOP SHELL RESTART',2)
    live_after=move(100,102)
    if live_restart['restarts']!=lang['restarts'] or live_after['restarts']!=live_restart['restarts']+1:
        raise ValueError('live TEA shell restart count')
    if any(live_after[k]!=live_restart[k] for k in ('text','text-hash','text-dirty','note-session')):
        raise ValueError('live TEA restart legacy state')
    live_frame=d.screen('extended-tea-live-restart')
    live_frame.check(81,81,'2579a1');live_frame.check(239,199,'2579a1')
    live_frame.check(281,81,'976332');live_frame.check(439,199,'976332')
    started=time.monotonic_ns()
    d.button(True);d.button(False);d.key('q')
    wait(proc,serial,b'STYLEOS TEA69 RETIRED',1)
    timings['first_close_ms']=(time.monotonic_ns()-started)/1e6
    move(300,100)
    peer=d.screen('extended-tea-peer')
    peer.check(281,81,'976332');peer.check(439,199,'976332')
    if peer.pixels[3*(peer.w*81+81):3*(peer.w*81+81)+3]==sample.pixels[3*(sample.w*81+81):3*(sample.w*81+81)+3]:
        raise ValueError('first app retained after retirement')
    started=time.monotonic_ns()
    d.button(True);d.button(False);d.key('q')
    wait(proc,serial,b'STYLEOS TEA69 RETIRED',2)
    timings['peer_close_ms']=(time.monotonic_ns()-started)/1e6
    (folder/'native-perf-timing.json').write_text(json.dumps({
        'scope':'host monotonic QEMU/QMP end-to-end; includes input and screenshot overhead',
        'milliseconds':timings},indent=2)+'\n')
    move(55,435)
    ended=d.screen('extended-tea-retired')
    if ended.pixels[3*(ended.w*81+281):3*(ended.w*81+281)+3]==peer.pixels[3*(peer.w*81+281):3*(peer.w*81+281)+3]:
        raise ValueError('peer pixels retained')
    if ended.pixels==sample.pixels:raise ValueError('native app retirement not rendered')
    if ended.pixels[3*(ended.w*81+81):3*(ended.w*81+81)+3]==sample.pixels[3*(sample.w*81+81):3*(sample.w*81+81)+3]:
        raise ValueError('native app pixels retained')
    (folder/'extended-desktop.json').write_text(json.dumps({'cursor':mv,'menu':opened,'closed':closed,'processes':procs,'restart':restart,'language':lang,'restored':restored,'live_restart':live_restart,'live_after':live_after,'frames':{x:hashlib.sha256((folder/(x+'.ppm')).read_bytes()).hexdigest() for x in ('extended-cursor-start','extended-cursor-end','extended-menu','extended-menu-closed','extended-processes','extended-window-max','extended-window-restore','extended-window-drag','extended-window-min','extended-window-taskbar','extended-window-close','extended-about','extended-about-close','extended-restart','extended-tea-live','extended-tea-both','extended-tea-live-restart','extended-tea-peer','extended-tea-retired')}},indent=2)+'\n')

def verify(folder,lines,root):
    from native_process_view_02 import verify_snapshot
    verify_snapshot(lines,live=False)
    data=json.loads((folder/'extended-desktop.json').read_text())
    actual=events(lines)
    for name in ('cursor','menu','closed','processes','restart','language','restored','live_restart','live_after'):
        receipt=data[name]
        if receipt['root']!=root or receipt not in actual:raise ValueError('extended receipt '+name)
    if lines.count(b'STYLEOS DESKTOP SHELL RESTART')!=2:raise ValueError('shell refresh marker')
    from native_forced_stop_verify import records
    admitted=records(lines,b'STYLEOS NATIVE SHELL-CONTROLLER',('pid','gen','root','epoch'))
    calls=records(lines,b'STYLEOS NATIVE SHELL-CALL',('pid','gen','root','op','reply'))
    applied=records(lines,b'STYLEOS NATIVE SHELL-APPLY',('epoch',))
    retired=records(lines,b'STYLEOS NATIVE SHELL-CONTROLLER-RETIRED',('pid','epoch','resident'))
    actions=records(lines,b'STYLEOS NATIVE SHELL-ACTION',('epoch','action'))
    submits=records(lines,b'STYLEOS NATIVE SHELL-SUBMIT',('key',))
    renders=[i for i,x in enumerate(lines) if x==b'STYLEOS SHELL ACTION RENDERED']
    if (len(admitted),len(calls),len(applied),len(retired),len(actions),len(submits),len(renders))!=(17,34,2,17,15,17,15):
        raise ValueError('ordinary controller lifecycle count')
    keys=(0x3e,0x3e,0x42,0x146,0x146,0x17e,0x16e,0x201,0x119,0x3e,0x302,0x101,0x309,0x44,0x30c,0x30c,0x44)
    if [x['key'] for _,x in submits]!=list(keys) or [tuple(x.values()) for _,x in actions]!=[(i,a) for i,a in enumerate((2,2,3,5,5,7,6,8,4,2,10,4,17,0,20,20,0),1) if a]:
        raise ValueError('ordinary command policy')
    for j,(i,a) in enumerate(admitted):
        pid,gen,child,epoch=a.values()
        expected_pid=j+1 if j<15 else 17 if j==15 else 19
        if (pid,gen,epoch)!=(expected_pid,expected_pid,j+1) or child==root or not child:raise ValueError('ordinary controller identity')
        if [tuple(x.values()) for _,x in calls[2*j:2*j+2]]!=[(pid,gen,child,0x105,(epoch<<32)|keys[j]),(pid,gen,child,0x106,epoch)] or tuple(retired[j][1].values())!=(pid,epoch,0 if j<15 else j-14):
            raise ValueError('ordinary controller syscall and cleanup')
        if not submits[j][0]<i<calls[2*j][0]<calls[2*j+1][0]:raise ValueError('ordinary controller order')
        if j<13 and not calls[2*j+1][0]<retired[j][0]<actions[j][0]<renders[j]<submits[j+1][0]:
            raise ValueError('command after retirement and actual render')
    if applied[0][1]['epoch']!=14 or not calls[27][0]<applied[0][0]<lines.index(b'STYLEOS DESKTOP SHELL RESTART'):
        raise ValueError('ordinary controller refresh ordering')
    launch=records(lines,b'STYLEOS TEA69 LAUNCHED',('pid','gen','root','window'))
    frame=records(lines,b'STYLEOS NATIVE FRAME',('pid','gen','root','window','va','count','status','first'))
    end=records(lines,b'STYLEOS TEA69 RETIRED',('pid','window'))
    if len(launch)!=2 or len(frame)!=2 or len(end)!=2:raise ValueError('TEA lifecycle')
    roots=set()
    for k,(i,l) in enumerate(launch):
        pid,gen,child,win=l.values()
        if (pid,gen,win)!=(16+2*k,16+2*k,1+k) or child==root or not child:raise ValueError('TEA identity')
        roots.add(child)
        color=(0x2579a1,0x976332)[k]
        if tuple(frame[k][1].values())!=(pid,gen,child,0,0x12000000,1,0,color) or tuple(end[k][1].values())!=(pid,win):
            raise ValueError('TEA frame and cleanup evidence')
        if not calls[29+2*k][0]<retired[14+k][0]<actions[-2+k][0]<i<frame[k][0]<end[k][0]:
            raise ValueError('TEA explicit launch ordering')
    restart_lines=[i for i,x in enumerate(lines) if x==b'STYLEOS DESKTOP SHELL RESTART']
    if applied[1][1]['epoch']!=17 or admitted[16][1]['root'] in roots or not frame[1][0]<submits[16][0]<calls[33][0]<applied[1][0]<restart_lines[1]<end[0][0]:
        raise ValueError('live TEA refresh identity/order')
    if len(roots)!=2 or not frame[0][0]<launch[1][0]<frame[1][0]<end[0][0]<end[1][0]:
        raise ValueError('TEA coexistence')
    resources=records(lines,b'STYLEOS TEA70 RESOURCES',('phase','pid','resident','pages','owners','windows','ports'))
    expected=[(1,16,1,3,1,1,1),(1,18,2,6,2,2,2),(2,16,1,3,1,1,1),(2,18,0,0,0,0,0)]
    if [tuple(v.values()) for _,v in resources]!=expected:
        raise ValueError('native resource accounting and release')
    (folder/'native-resource-accounting.json').write_text(json.dumps({
        'scope':'native leased user pages only; excludes kernel, tables, surfaces and caches',
        'page_bytes':4096,'snapshots':[v for _,v in resources]},indent=2)+'\n')
    if lines.count(b'STYLEOS SHELL PROVIDER READY')!=1:raise ValueError('ordinary provider readiness')
    if not any(x==b'STYLEOS DESKTOP CURSOR DAMAGE PASS' for x in lines):raise ValueError('cursor proof missing')
    for name,expected in data['frames'].items():
        if hashlib.sha256((folder/(name+'.ppm')).read_bytes()).hexdigest()!=expected:raise ValueError('UI frame digest')
