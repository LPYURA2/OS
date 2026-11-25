"""Synthetic CPU records derived from actual earlier S849 UART, or genuine 860 replay."""
import gzip,json,re,sys
from pathlib import Path
from runtime_tick_860_verify import verify_trace,verify,HEADER,ORDER
from fx_context_849_verify import verify as prior
p=Path(sys.argv[1]);raw=p.read_bytes()
if len(sys.argv)>2:
    ok,obs=verify(raw,33);assert ok,obs
    trace=gzip.decompress(Path(sys.argv[2]).read_bytes()).decode('ascii');scope='actual S860 CPU trace'
else:
    ok,obs=prior(raw,33);assert ok,obs;roots=obs['child_roots'];sel=obs['process_selectors'];rows=[];ti=ei=0
    for step,id in enumerate(ORDER):
        if step in (6,9):
            v=obs['user_preempt_ends'][ei];ei+=1;vec,err,soft,rip=v['vector'],v['error'],int(id==1),v['rip']-(2 if id else 0)
        else:
            v=obs['user_preempt_ticks'][ti];ti+=1;vec,err,soft,rip=0x50,0,0,v['rip']
        rows.append(f' {step+100}: v={vec:02x} e={err:04x} i={soft} cpl=3 IP={sel["cs"]:04x}:{rip:016x} pc={rip:016x} SP={sel["ss"]:04x}:0000000011001000\nCR3={roots["ab"[id]]:016x}\n')
    trace=''.join(rows);scope='synthetic CPU trace on earlier S849 UART (NOT S860 guest)'
records=verify_trace(trace,obs);assert len(records)==10
heads=list(HEADER.finditer(trace))[-10:];n=0
def reject(s):
    global n
    try:verify_trace(s,obs)
    except ValueError:n+=1;return
    raise AssertionError('accepted CPU trace mutation')
for m in heads:
    for group in range(2,11):
        a,z=m.span(group);old=trace[a:z]
        for bit in range(4*len(old)):
            reject(trace[:a]+f'{int(old,16)^(1<<bit):0{len(old)}x}'+trace[z:])
    h=re.search(r'\bCR3=([0-9a-fA-F]+)\b',trace[m.end():]);a,z=(m.end()+i for i in h.span(1));old=trace[a:z]
    for bit in range(4*len(old)):reject(trace[:a]+f'{int(old,16)^(1<<bit):0{len(old)}x}'+trace[z:])
reject(trace[:heads[-1].start()]);reject(trace+trace[heads[-1].start():]);reject('')
print(scope,': 1 positive /',n,'refusals PASS')
