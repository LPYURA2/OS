#!/usr/bin/env python3
"""Recheck original S861 UART, binary/map and CPU events; mutate actual evidence."""
import gzip, hashlib, json, re, sys
from pathlib import Path
from desktop_switch_851_site import bp_site
from irq_trace_accept_859 import HEADER
from runtime_roots_861_verify import verify, trace_check

def main(p):
    b=p/'build/x86_64'; res=json.loads((p/'results.json').read_text())
    raw=(p/'serial.log').read_bytes(); efi=(b/'BOOTX64.EFI').read_bytes()
    m=(b/'kernel.map').read_bytes(); data=gzip.decompress((p/'cpu-trace.log.gz').read_bytes())
    for key,v in [('uart_sha256',raw),('efi_sha256',efi),('map_sha256',m),('trace_sha256',data)]:
        if hashlib.sha256(v).hexdigest()!=res[key]:raise ValueError('capture hash '+key)
    site=bp_site(efi,m.decode());site['child_rva']=bp_site(efi,m.decode(),symbol='s861_bp_return')['rva']
    if site!=res['site'] or res['status']!='PASS':raise ValueError('site/status')
    reference=json.loads((p/'reference.json').read_text()); trace=data.decode('ascii')
    ok,obs=verify(raw,res['qemu_exit_code'],site,reference)
    if not ok or obs!=res['observations']:raise ValueError('UART replay')
    events=trace_check(trace,obs)
    if json.loads(json.dumps(events))!=res['cpu_events']:raise ValueError('CPU replay')
    headers=list(HEADER.finditer(trace)); first=next(h for h in headers if int(h[1])==events[0][0]); prefix,tail=trace[:first.start()],trace[first.start():]
    heads=list(HEADER.finditer(tail)); n=0; u=0
    def reject(t):
        nonlocal n
        try:trace_check(prefix+t,obs)
        except ValueError:n+=1;return
        raise AssertionError('accepted CPU mutation')
    def bits(a,z):
        old=tail[a:z]
        for i in range(4*len(old)):
            reject(tail[:a]+f'{int(old,16)^(1<<i):0{len(old)}x}'+tail[z:])
    for h in heads:
        for group in range(2,11):bits(*h.span(group))
    for pat in (r'\bCR3=([0-9a-fA-F]+)',r'^RIP=([0-9a-fA-F]+)',r'\bRSP=([0-9a-fA-F]+)',r'\bRFL=([0-9a-fA-F]+)',r'^GDT=\s+([0-9a-fA-F]+)',r'^IDT=\s+([0-9a-fA-F]+)',r'^TR\s*=([0-9a-fA-F]+)'):
        for h in re.finditer(pat,tail,re.M):bits(*h.span(1))
    blocks=[tail[h.start():heads[i+1].start() if i+1<len(heads) else len(tail)] for i,h in enumerate(heads)]
    for i in range(3):
        reject(''.join(blocks[:i]+blocks[i+1:]))
        reject(''.join(blocks[:i]+[blocks[i]]+blocks[i:]))
    reject(blocks[1]+blocks[0]+blocks[2]);reject(blocks[0]+blocks[2]+blocks[1])
    # UART-only changes to legitimate RSP/flags must also be rejected by CPU binding.
    def reject_uart(v):
        nonlocal u
        ok,o=verify(v,res['qemu_exit_code'],site,reference)
        if ok:
            try:trace_check(trace,o)
            except ValueError:pass
            else:raise AssertionError('accepted UART mutation')
        u+=1
    for h in re.finditer(rb'^STYLEOS S861 (?:ROOTS|SWITCH|CLEAN) [^\r\n]+',raw,re.M):
        for field in re.finditer(rb'=0x([0-9a-f]{16})',h[0]):
            a,z=(h.start()+i for i in field.span(1));old=raw[a:z]
            for i in range(64):reject_uart(raw[:a]+f'{int(old,16)^(1<<i):016x}'.encode()+raw[z:])
        reject_uart(raw[:h.start()]+raw[h.end():]);reject_uart(raw[:h.start()]+h[0]+b'\n'+raw[h.start():])
    result={'status':'PASS','events':events,'actual_cpu_refusals':n,'actual_uart_refusals':u,'trace_sha256':res['trace_sha256']}
    (p/'independent-replay.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'S861 original replay: {len(events)} real CPU events; {n} CPU and {u} UART refusals PASS')

if __name__=='__main__':main(Path(sys.argv[1]))
