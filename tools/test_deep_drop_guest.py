"""Deep Drop on the ordinary ISO, with two pinned external CPL3 applications."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import tempfile

from block_fixture_856_verify import CONTROLLER, DEVICE
from irq_trace_accept_859 import HEADER
from make_desktop_data import provision
from native_forced_stop_verify import records
from test import firmware_path
from test_desktop_v0 import Desktop
from uart_receipts import complete_lines

CALL = b'STYLEOS DD72 CALL'
CALL_KEYS = ('pid', 'gen', 'root', 'op', 'id', 'status', 'result')
LAUNCH = b'STYLEOS TEA69 LAUNCHED'
IDENTITY = ('pid', 'gen', 'root', 'window')
FRAME = b'STYLEOS NATIVE FRAME'
FRAME_KEYS = ('pid', 'gen', 'root', 'window', 'va', 'count', 'status', 'first')
OFFER, ACCEPT, READ, FINISH, CANCEL = range(0x110, 0x115)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def color(text):
    value = 2166136261
    for byte in text:
        value = (value ^ byte) * 16777619 & 0xffffffff
    return value & 0xffffff

def provision_clients(build, disk):
    provision(disk)
    with disk.open('r+b') as out:
        out.seek(4095 * 512)
        out.write(b'STYLEOS-S864-PROVISIONED-V1'.ljust(512, b'\0'))
        for name, lba in [('Drop-source', 2960), ('Drop-target', 3000)]:
            blob = (build / (name + '.tea')).read_bytes()
            if len(blob) > 16384 or blob in (build / 'BOOTX64.EFI').read_bytes():
                raise ValueError('external package bounds or provenance')
            header = struct.pack('<8sII32s', b'STEA769\0', 1, len(blob), hashlib.sha256(blob).digest())
            out.seek(lba * 512)
            out.write(header.ljust(512, b'\0') + blob)

def verify_trace(trace, calls, faults):
    data = trace.read_text()
    headers = list(HEADER.finditer(data))
    actual, exceptions = [], Counter()
    for i, head in enumerate(headers):
        vector = int(head[2], 16)
        if int(head[5]) != 3 or vector not in (6, 128):
            continue
        block = data[head.start():headers[i+1].start() if i+1 < len(headers) else len(data)]
        root = int(re.search(r'\bCR3=([0-9a-fA-F]+)', block)[1], 16)
        if vector == 6:
            if head[4] != '0' or int(head[3], 16) != 0:
                raise ValueError('invalid #UD trace')
            exceptions[root] += 1
            continue
        op = int(re.search(r'\bRAX=([0-9a-fA-F]+)', block)[1], 16)
        if OFFER <= op <= CANCEL:
            if head[4] != '1' or int(head[3], 16) or int(head[6], 16) != 0x5b or int(head[9], 16) != 0x63:
                raise ValueError('invalid CPL3 INT 80 trace')
            token = int(re.search(r'\bRBX=([0-9a-fA-F]+)', block)[1], 16)
            actual.append((root, op, token))
    assert actual == [(v['root'], v['op'], v['id']) for _, v in calls], 'CPL3 trace / broker mismatch'
    assert exceptions == Counter(faults), 'actual CPL3 faults'
    return len(actual)

class Run:
    def __init__(self, desktop, serial, folder):
        self.d, self.serial, self.folder = desktop, serial, folder
        self.point = 0
        self.memory_checks = []

    def lines(self):
        return complete_lines(self.serial.read_bytes()) if self.serial.exists() else []

    def mark(self):
        self.point = len(self.lines())

    def until(self, predicate, label, timeout=35):
        end = time.monotonic() + timeout
        while time.monotonic() < end and self.d.proc.poll() is None:
            lines = self.lines()
            if any(b' FAIL ' in s or b'BOUNDARY ERROR' in s for s in lines):
                break
            found = predicate(lines[self.point:])
            if found:
                return found
            time.sleep(.03)
        raise RuntimeError(f'{label}: last UART\n' + '\n'.join(
            line.decode(errors='replace') for line in self.lines()[-15:]))

    def receipt(self, tag, keys, **want):
        def find(lines):
            return next((r for _, r in records(lines, tag, keys)
                         if all(r[k] == v for k, v in want.items())), None)
        return self.until(find, f'{tag!r} {want}')

    def line(self, exact):
        self.until(lambda lines: exact in lines, str(exact))

    def launch(self):
        self.mark()
        self.d.key('f7')
        app = self.receipt(LAUNCH, IDENTITY)
        self.receipt(FRAME, FRAME_KEYS, pid=app['pid'], gen=app['gen'], root=app['root'], status=0)
        self.park(app)
        return app

    def park(self, app):
        self.receipt(b'STYLEOS TEA70 PARK', ('pid','gen'), pid=app['pid'], gen=app['gen'])

    def call(self, app, op, status, **extra):
        match = self.receipt(CALL, CALL_KEYS, pid=app['pid'], gen=app['gen'],
                             root=app['root'], op=op, status=status, **extra)
        if status and match['result']:
            raise ValueError('refused syscall returned data')
        return match

    def gesture(self, source, target):
        self.d.move(105, 105)
        self.mark()
        self.d.button(True)
        self.d.move(310, 105)
        self.d.button(False)
        self.line(b'STYLEOS DD72 GESTURE')
        transfer = self.call(source, OFFER, 0)['result']
        assert transfer
        self.call(source, ACCEPT, 2, id=transfer)
        self.receipt(b'STYLEOS TEA70 INPUT-COUNTS', ('pid','gen','empty','delivered','other','kind','code'),
                     pid=target['pid'], gen=target['gen'], kind=4, code=2)
        self.park(target)
        return transfer

    def key(self, position, name):
        self.d.click(*position)
        self.mark()
        self.d.key(name)

    def frame(self, app, text, position, name):
        expected = color(text)
        self.receipt(FRAME, FRAME_KEYS, pid=app['pid'], gen=app['gen'], root=app['root'],
                     status=0, first=expected)
        self.park(app)
        self.d.screen(name).check(*position, f'{expected:06x}')

    def private_text(self, source, selected, target, document):
        # Walk the guest's live CR3s while the VM is paused. Read physical
        # memory independently of the app checksum, receipts and kernel aliases.
        report = []
        self.d.call('stop')
        try:
            with tempfile.TemporaryDirectory(dir=self.folder) as directory:
                span = Path(directory) / 'span'
                def read_pa(pa, size):
                    self.d.call('pmemsave', val=pa, size=size, filename=str(span))
                    data = span.read_bytes()
                    assert len(data) == size
                    return data
                for app, expected in ((source,selected), (target,document)):
                    va = 0x12000000
                    page = app['root']
                    for shift in (39,30,21,12):
                        pte = int.from_bytes(read_pa(page + 8*((va >> shift)&511),8),'little')
                        assert pte&7 == 7 and (shift==12 or not pte&128), 'private user mapping'
                        page = pte & 0x000ffffffffff000
                    assert pte >> 63 == 1, 'private data must be NX'
                    data = read_pa(page + 1024,len(expected)+1)
                    assert data == expected + b'\0', 'private document bytes or trailing guard'
                    report.append(dict(pid=app['pid'],generation=app['gen'],root=app['root'],
                                       data_page=page,length=len(expected),
                                       sha256=hashlib.sha256(data[:-1]).hexdigest()))
            assert report[0]['data_page'] != report[1]['data_page']
            self.memory_checks.append(report)
        finally:
            self.d.call('cont')

    def stop_via_processes(self, app):
        self.mark()
        self.d.key('f8')
        self.line(b'STYLEOS PROCESSES END')
        from native_process_view_02 import decimal, ROW, HEADER as TABLE
        lines = self.lines()[self.point:]
        start = max(i for i,x in enumerate(lines) if x.startswith(TABLE+b' '))
        rows = [decimal(x,ROW,(b'pid',b'group',b'state',b'ticks',b'bytes'))
                for x in lines[start+1:] if x.startswith(ROW+b' ')]
        pids = [r[0] for r in rows]
        assert len(pids) == 2 and app['pid'] in pids
        for _ in range(pids.index(app['pid']) + 1):
            self.d.key('down')
        self.d.key('delete')
        self.line(f"STYLEOS PROCESSES ARM pid={app['pid']}".encode())
        from native_desktop_acceptance import events
        self.until(lambda lines: any(e['key']==0x153 and e['writes'] for e in events(lines)),
                   'confirmation rendered')
        self.d.key('ret')
        self.line(f"STYLEOS PROCESSES STOP pid={app['pid']} status=0 remaining=1".encode())

def exercise(r):
    left, right = (105, 105), (310, 105)
    source, target = r.launch(), r.launch()
    assert all(source[k] != target[k] for k in IDENTITY)
    selected = 'StyleOS / Чай '.encode()
    doc = b''
    r.key(left,'g')
    r.call(source,OFFER,4)
    transfers, faults = [], []
    for i, char in enumerate('aba'):
        r.key(left,char)
        selected += char.encode()
        token = r.gesture(source,target)
        transfers.append(token)
        r.private_text(source,selected,target,doc)
        if i == 0:
            r.mark()
            r.d.key('f10')
            r.line(b'STYLEOS DESKTOP SHELL RESTART')
        r.key(right,'ret')
        r.call(target,ACCEPT,0,id=token)
        r.call(target,READ,1,id=token)
        r.call(target,READ,5,id=token)
        r.call(target,READ,0,id=token,result=len(selected))
        r.call(target,FINISH,0,id=token)
        r.call(target,READ,4,id=token)
        doc += selected
        r.frame(target,doc,(281,81),f'target-{i}')
        r.key(left,'v')
        r.frame(source,selected,(81,81),f'source-{i}')
        r.private_text(source,selected,target,doc)
    r.mark()
    r.d.key('f10')
    r.line(b'STYLEOS DESKTOP SHELL RESTART')
    r.key(right,'v')
    r.frame(target,doc,(281,81),'after-refresh')
    r.private_text(source,selected,target,doc)
    for pos,app in [(left,source),(right,target)]:
        token = r.gesture(source,target)
        transfers.append(token)
        r.key(pos,'esc')
        r.call(app,CANCEL,0,id=token)
        r.key(right,'r')
        r.call(target,READ,4,id=token)
        r.private_text(source,selected,target,doc)
    for role, mode in [('source','fault'),('target','fault'),('source','stop'),('target','stop')]:
        token = r.gesture(source,target)
        transfers.append(token)
        victim, peer = (source,target) if role=='source' else (target,source)
        if mode=='fault':
            r.key(left if role=='source' else right,'x')
            faults.append(victim['root'])
        else:
            r.stop_via_processes(victim)
        r.line(b'STYLEOS DD72 REVOKED')
        r.receipt(b'STYLEOS TEA69 RETIRED',('pid','window'),pid=victim['pid'],window=victim['window'])
        r.receipt(b'STYLEOS TEA70 RESOURCES',('phase','pid','resident','pages','owners','windows','ports'),
                  phase=2,pid=victim['pid'],resident=1,pages=3,owners=1,windows=1,ports=1)
        r.key(right if role=='source' else left,'v')
        r.frame(peer,doc if role=='source' else selected,
                (281,81) if role=='source' else (81,81),f'peer-{role}-{mode}')
        if role=='source':
            r.key(right,'r')
            r.call(peer,READ,4,id=token)
        else:
            r.key(left,'esc')
            r.call(peer,CANCEL,4,id=token)
        replacement = r.launch()
        assert all(replacement[k]!=victim[k] for k in ('pid','gen','window'))
        if role=='source':
            source,selected = replacement,'StyleOS / Чай '.encode()
        else:
            target,doc = replacement,b''
        r.private_text(source,selected,target,doc)
    # No kernel-directed time advance and no input while the endpoints wait.
    token = r.gesture(source,target)
    transfers.append(token)
    r.mark()
    r.until(lambda lines: b'STYLEOS DD72 REVOKED' in lines, 'trusted deadline', timeout=210)
    assert not records(r.lines()[r.point:],CALL,CALL_KEYS)
    r.key(right,'ret')
    r.call(target,ACCEPT,4,id=token)
    r.private_text(source,selected,target,doc)
    # The most recent target is a fresh process with an empty document.
    assert doc==b''
    r.key(left,'m')
    selected=bytes(65+i%26 for i in range(1024))
    token=r.gesture(source,target)
    transfers.append(token)
    r.key(right,'ret')
    r.call(target,READ,0,id=token,result=1024)
    r.call(target,FINISH,0,id=token)
    r.call(target,READ,4,id=token)
    r.frame(target,selected,(281,81),'target-1024')
    r.private_text(source,selected,target,selected)
    for pos,app in [(left,source),(right,target)]:
        r.key(pos,'q')
        r.receipt(b'STYLEOS TEA69 RETIRED',('pid','window'),pid=app['pid'],window=app['window'])
    r.receipt(b'STYLEOS TEA70 RESOURCES',('phase','pid','resident','pages','owners','windows','ports'),
              phase=2,pid=target['pid'],resident=0,pages=0,owners=0,windows=0,ports=0)
    assert transfers == sorted(set(transfers))
    return transfers, faults

def run(build, out):
    build, out = build.resolve(), out.resolve()
    out.mkdir(parents=True,exist_ok=False)
    iso = build / 'StyleOS-owned-shell-865.iso'
    disk = out / 'Deep-Drop-clients.raw'
    provision_clients(build,disk)
    before = digest(disk)
    vars_path = out/'OVMF_VARS.fd'
    shutil.copyfile(firmware_path('OVMF_VARS','OVMF_VARS'),vars_path)
    qemu = shutil.which('qemu-system-x86_64')
    version = subprocess.check_output([qemu,'--version'],text=True).splitlines()[0]
    if not version.startswith('QEMU emulator version 8.2.2 '):
        raise RuntimeError('validated QEMU 8.2.2 required')
    serial, trace = out/'serial.log', out/'cpu-trace.log'
    cmd = [qemu,'-machine','q35','-accel','tcg,thread=single','-smp','1','-m','128M',
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
            d = Desktop(proc,serial,out)
            assert 'QMP' in d.receive()
            d.call('qmp_capabilities')
            d.call('cont')
            r = Run(d,serial,out)
            r.line(b'STYLEOS DESKTOP READY')
            d.x,d.y = 640,600
            transfers,faults = exercise(r)
            d.call('stop')
            stats = next(item['stats'] for item in d.call('query-blockstats')
                         if item.get('qdev','').endswith(DEVICE) or item.get('device')=='data')
            assert stats['wr_operations']==0 and stats['wr_bytes']==0
            d.call('quit')
            assert proc.wait(timeout=15)==0
        finally:
            if proc.poll() is None:
                proc.kill()
    lines = complete_lines(serial.read_bytes())
    calls = records(lines,CALL,CALL_KEYS)
    count = verify_trace(trace,calls,faults)
    assert digest(disk)==before
    assert len([v for _,v in calls if v['op']==FINISH and v['status']==0])==4
    result = dict(status='PASS',qemu=version,iso_sha256=digest(iso),data_sha256=before,
                  packages={p.name:digest(p) for p in build.glob('Drop-*.tea')},
                  offers=transfers,insertions=4,cancellations=2,expired=1,max_text_bytes=1024,endpoint_faults=2,forced_stops=2,
                  verified_cpl3_calls=count,guest_disk_writes=0,residual_leased_app_pages=0,
                  private_memory=r.memory_checks,uart_sha256=digest(serial),trace_sha256=digest(trace))
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    run(args.build,args.output)
