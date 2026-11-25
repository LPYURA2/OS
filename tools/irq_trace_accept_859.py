#!/usr/bin/env python3
"""Bind the QEMU CPU-event trace to the already verified S859 UART/PE proof."""
import gzip,hashlib,json,re,sys
from pathlib import Path
from runtime_irq_859_verify import verify,wait_site
from irq_publication_859 import publication_site
from desktop_switch_851_site import bp_site
HEADER=re.compile(r'^\s*(\d+): v=([0-9a-fA-F]{2}) e=([0-9a-fA-F]+) i=([01]) cpl=([0-3]) IP=([0-9a-fA-F]+):([0-9a-fA-F]+) pc=([0-9a-fA-F]+) SP=([0-9a-fA-F]+):([0-9a-fA-F]+)\b',re.M)

def trace_check(trace,obs):
    root=obs['active']['root']; records=[]; started=False
    headers=list(HEADER.finditer(trace))
    for i,m in enumerate(headers):
        block=trace[m.start():headers[i+1].start() if i+1<len(headers) else len(trace)]
        def value(pattern):
            v=re.findall(pattern,block,re.M)
            if len(v)!=1:raise ValueError('missing or duplicate trace field: '+pattern)
            return int(v[0],16)
        # QEMU logs firmware events too. Only the terminal owned-root path is admitted.
        cr3=value(r'\bCR3=([0-9a-fA-F]+)\b')
        if not started and cr3!=root:continue
        started=True
        seq,vec,err,soft,cpl,cs,rip,pc,ss,rsp=m.groups()
        rec={'sequence':int(seq),'vector':int(vec,16),'error':int(err,16),'software':int(soft),'cpl':int(cpl),'cs':int(cs,16),'rip':int(rip,16),'pc':int(pc,16),'ss':int(ss,16),'rsp':int(rsp,16),'cr3':cr3,'flags':value(r'^RIP=[0-9a-fA-F]+ RFL=([0-9a-fA-F]+)\b'),'trace-rip':value(r'^RIP=([0-9a-fA-F]+)\b'),'trace-rsp':value(r'\bRSP=([0-9a-fA-F]+)\b'),'gdt':value(r'^GDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),'idt':value(r'^IDT=\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*$'),'tr':value(r'^TR\s*=([0-9a-fA-F]+)\b')}
        records.append(rec)
    if len(records)!=2:raise ValueError('expected exactly two events from owned-root breakpoint through exit')
    if records[1]['sequence']!=records[0]['sequence']+1:raise ValueError('event sequence')
    for r,v,soft,proof,rip in ((records[0],3,1,obs['bp'],obs['bp']['rip']-1),(records[1],0x50,0,obs['irq'],obs['irq']['rip'])):
        if tuple(r[k] for k in ('vector','error','software','cpl','cs','rip','pc','rsp','cr3','flags','trace-rip','trace-rsp','gdt','idt','tr'))!=(v,0,soft,0,proof['cs'],rip,rip,proof['rsp'],root,proof['flags'],rip,proof['rsp'],obs['active']['gdt'],obs['active']['idt'],obs['active']['tr']):raise ValueError('event/descriptor/frame mismatch')
    return records

def main(p):
    b=p/'build/x86_64';res=json.loads((p/'results.json').read_text());raw=(p/'serial.log').read_bytes();efi=(b/'BOOTX64.EFI').read_bytes();m=(b/'kernel.map').read_text();ref=json.loads((p/'reference.json').read_text())
    for data,key in ((raw,'uart_sha256'),(efi,'efi_sha256'),(m.encode(),'map_sha256')):
        if hashlib.sha256(data).hexdigest()!=res[key]:raise ValueError('capture hash '+key)
    site=bp_site(efi,m);site.update(wait_site(efi,m));site.update(publication_site(efi,m))
    if site!=res['site'] or res['status']!='PASS':raise ValueError('PE site/status mismatch')
    ok,obs=verify(raw,res['qemu_exit_code'],site,ref)
    if not ok or obs!=res['observations']:raise ValueError('original UART replay')
    trace=gzip.decompress((p/'cpu-trace.log.gz').read_bytes());text=trace.decode('ascii');records=trace_check(text,obs)
    # Actual trace mutation test: every header event vector, error, software,
    # privilege, IP, PC, stack, root, flags and descriptor value must bind.
    start=text.index(next(m.group(0) for m in HEADER.finditer(text) if int(m.group(1))==records[0]['sequence']))
    prefix,tail=text[:start],text[start:];n=0
    def reject(s):
        nonlocal n
        try:trace_check(s,obs)
        except ValueError:n+=1;return
        raise AssertionError('accepted trace mutation')
    heads=list(HEADER.finditer(tail))
    for h in heads:
        for group in (2,3,4,5,6,7,8,10):
            a,z=h.span(group);old=tail[a:z]
            for bit in range(4*len(old)):
                new=f'{int(old,16)^(1<<bit):0{len(old)}x}'
                reject(prefix+tail[:a]+new+tail[z:])
    for pat in (r'\bCR3=([0-9a-fA-F]+)',r'^RIP=([0-9a-fA-F]+)',r'\bRSP=([0-9a-fA-F]+)',r'\bRFL=([0-9a-fA-F]+)',r'^GDT=\s+([0-9a-fA-F]+)',r'^IDT=\s+([0-9a-fA-F]+)',r'^TR\s*=([0-9a-fA-F]+)'):
        for h in re.finditer(pat,tail,re.M):
            a,z=h.span(1);old=tail[a:z]
            for bit in range(4*len(old)):
                reject(prefix+tail[:a]+f'{int(old,16)^(1<<bit):0{len(old)}x}'+tail[z:])
    reject(prefix);reject(prefix+tail[:heads[1].start()]);reject(prefix+tail+tail[heads[1].start():]);reject(prefix+tail.replace('v=50 e=0000','v=0e e=0000',1))
    out={'trace_sha256':hashlib.sha256(trace).hexdigest(),'events':records,'actual_trace_refusals':n,'status':'PASS'}
    (p/'cpu-trace-acceptance.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
if __name__=='__main__':main(Path(sys.argv[1]))
