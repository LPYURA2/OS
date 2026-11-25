#!/usr/bin/env python3
"""Replay integrated S861 actual capture and challenge its evidence bindings."""
import gzip, hashlib, json, re, sys
from pathlib import Path
from runtime_loop_862_verify import verify, trace_check, loop_site
from irq_trace_accept_859 import HEADER

def main(p):
    r=json.loads((p/'results.json').read_text()); b=p/'build/x86_64'
    raw=(p/'serial.log').read_bytes(); efi=(b/'BOOTX64.EFI').read_bytes(); m=b/'kernel.map'
    data=gzip.decompress((p/'cpu-trace.log.gz').read_bytes()); trace=data.decode('ascii')
    for key,value in [('uart',raw),('efi',efi),('map',m.read_bytes()),('trace',data)]:
        if hashlib.sha256(value).hexdigest()!=r[key+'_sha256']:raise ValueError('hash '+key)
    site=loop_site(efi,m); ref=json.loads((p/'reference.json').read_text())
    if site!=r['site'] or r['status']!='PASS':raise ValueError('site/status')
    ok,obs=verify(raw,r['qemu_exit_code'],site,ref)
    if not ok or obs!=r['observations']:raise ValueError('UART replay')
    events=trace_check(trace,obs)
    if json.loads(json.dumps(events))!=r['cpu_events']:raise ValueError('CPU replay')
    start=next(h.start() for h in HEADER.finditer(trace) if int(h[1])==events[0][0]); prefix,tail=trace[:start],trace[start:]
    heads=list(HEADER.finditer(tail)); n=u=0
    def reject(t):
        nonlocal n
        try:trace_check(t,obs)
        except ValueError:n+=1;return
        raise AssertionError('accepted CPU mutation')
    def bits(a,z):
        old=tail[a:z]
        for i in range(4*len(old)):reject(tail[:a]+f'{int(old,16)^(1<<i):0{len(old)}x}'+tail[z:])
    for h in heads:
        for group in range(2,11):bits(*h.span(group))
    for pat in (r'\bCR3=([0-9a-fA-F]+)',r'^RIP=([0-9a-fA-F]+)',r'\bRSP=([0-9a-fA-F]+)',r'\bRFL=([0-9a-fA-F]+)',r'^GDT=\s+([0-9a-fA-F]+)',r'^IDT=\s+([0-9a-fA-F]+)',r'^TR\s*=([0-9a-fA-F]+)'):
        for h in re.finditer(pat,tail,re.M):bits(*h.span(1))
    blocks=[tail[h.start():heads[i+1].start() if i+1<len(heads) else len(tail)] for i,h in enumerate(heads)]
    for i in range(len(blocks)):
        reject(''.join(blocks[:i]+blocks[i+1:]));reject(''.join(blocks[:i]+[blocks[i]]+blocks[i:]))
        if i+1<len(blocks):reject(''.join(blocks[:i]+[blocks[i+1],blocks[i]]+blocks[i+2:]))
        if int(heads[i][2],16)==14:
            h=re.search(r'^CR0=[^\n]*\bCR2=([0-9a-fA-F]+)',blocks[i],re.M)
            a,z=h.span(1);bits(heads[i].start()+a,heads[i].start()+z)
    def reject_uart(v):
        nonlocal u
        ok,o=verify(v,r['qemu_exit_code'],site,ref)
        if ok:
            try:trace_check(tail,o)
            except ValueError:pass
            else:raise AssertionError('accepted UART mutation')
        u+=1
    for h in re.finditer(rb'^STYLEOS (?:S862B (?:ENTER|COMMIT|CLEAN)|S862L (?:READY|EMPTY)) [^\r\n]+',raw,re.M):
        for field in re.finditer(rb'=0x([0-9a-f]{16})',h[0]):
            a,z=(h.start()+i for i in field.span(1));old=raw[a:z]
            for i in range(64):reject_uart(raw[:a]+f'{int(old,16)^(1<<i):016x}'.encode()+raw[z:])
        reject_uart(raw[:h.start()]+raw[h.end():]);reject_uart(raw[:h.start()]+h[0]+b'\n'+raw[h.start():])
    result={'status':'PASS','events':events,'actual_cpu_refusals':n,'actual_uart_refusals':u,'trace_sha256':r['trace_sha256']}
    (p/'independent-replay.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'S862 loop original replay: {len(events)} real CPU events; {n} CPU and {u} UART refusals PASS')

if __name__=='__main__':main(Path(sys.argv[1]))
