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
    restart=key('f10',0x44); rf=d.screen('extended-restart')
    if restart['restarts']!=before['restarts']+1 or any(restart[k]!=before[k] for k in ('text','text-hash','text-dirty','note-session')):raise ValueError('shell restart state')
    lang=key('f6',0x40); restored=key('f6',0x40)
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
    if not any(x==b'STYLEOS DESKTOP CURSOR DAMAGE PASS' for x in lines):raise ValueError('cursor proof missing')
    for name,expected in data['frames'].items():
        if hashlib.sha256((folder/(name+'.ppm')).read_bytes()).hexdigest()!=expected:raise ValueError('UI frame digest')
