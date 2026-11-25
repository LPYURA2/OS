"""Synthetic CPU records derived from actual earlier S849 UART, or genuine 860 replay."""
import gzip,re,sys
from pathlib import Path
from runtime_tick_860_verify import verify_trace,verify,HEADER,ORDER
from fx_context_849_verify import verify as prior
p=Path(sys.argv[1]);raw=p.read_bytes()
if len(sys.argv)>2:
    ok,obs=verify(raw,33);assert ok,obs
    trace=gzip.decompress(Path(sys.argv[2]).read_bytes()).decode('ascii');scope='actual S860 CPU trace'
else:
    ok,obs=prior(raw,33);assert ok,obs;roots=obs['child_roots'];sel=obs['process_selectors'];rows=[];ti=ei=0
    def add(vec,err,soft,cpl,cs,rip,ss,rsp,root):
        rows.append(f' {len(rows)+100}: v={vec:02x} e={err:04x} i={soft} cpl={cpl} IP={cs:04x}:{rip:016x} pc={rip:016x} SP={ss:04x}:{rsp:016x}\nCR3={root:016x}\n')
    for v in obs['process_traps']:
        soft=int(v['vector']==128);add(v['vector'],v['error'],soft,3,v['cs'],v['rip']-2*soft,v['ss'],v['rsp'],v['root'])
    for step,id in enumerate(ORDER):
        if step in (6,9):
            v=obs['user_preempt_ends'][ei];ei+=1;vec,err,soft,rip=v['vector'],v['error'],int(id==1),v['rip']-(2 if id else 0)
        else:
            v=obs['user_preempt_ticks'][ti];ti+=1;vec,err,soft,rip=0x50,0,0,v['rip']
        add(vec,err,soft,3,sel['cs'],rip,sel['ss'],0x11001000,roots['ab'[id]])
    for tick in obs['ticks']:
        for v in sorted((x for x in obs.get('deferred_irqs',[]) if x['n']==tick['n']),key=lambda x:x['count'])+[tick]:
            add(0x50,0,0,0,0x38,v['rip'],0x30,v['rsp'],v['root'])
    trace=''.join(rows);scope='synthetic CPU trace on earlier S849 UART (NOT S860 guest)'
records=verify_trace(trace,obs);count=34+len(obs.get('deferred_irqs',[]));assert len(records)==count
# Full capture is checked above; mutate its complete verified suffix for speed.
start=list(HEADER.finditer(trace))[-count].start();suffix=trace[start:]
assert verify_trace(suffix,obs)==records
heads=list(HEADER.finditer(suffix));n=0
def reject(s):
    global n
    try:verify_trace(s,obs)
    except ValueError:n+=1;return
    raise AssertionError('accepted CPU trace mutation')
for i,m in enumerate(heads):
    end=heads[i+1].start() if i+1<len(heads) else len(suffix)
    for group in range(2,11):
        a,z=m.span(group);old=suffix[a:z]
        for bit in range(4*len(old)):
            reject(suffix[:a]+f'{int(old,16)^(1<<bit):0{len(old)}x}'+suffix[z:])
    h=re.search(r'\bCR3=([0-9a-fA-F]+)\b',suffix[m.end():end]);a,z=(m.end()+j for j in h.span(1));old=suffix[a:z]
    for bit in range(4*len(old)):reject(suffix[:a]+f'{int(old,16)^(1<<bit):0{len(old)}x}'+suffix[z:])
    reject(suffix[:m.start()]+suffix[end:])
    reject(suffix[:m.start()]+suffix[m.start():end]+suffix[m.start():])
    a,z=m.span(1);reject(suffix[:a]+str(int(m[1])+1)+suffix[z:])
    if i+1<len(heads):
        next_end=heads[i+2].start() if i+2<len(heads) else len(suffix)
        reject(suffix[:m.start()]+suffix[end:next_end]+suffix[m.start():end]+suffix[next_end:])
reject('')
reject(suffix[:heads[18].start()]) # missing entire S843 tail
reject(suffix[heads[8].start():heads[18].start()]) # old ten-event assumption
print(scope,':',count,'matched events /',n,'refusals PASS')
