"""Unit tests of new UART binding only: NOT S862 execution evidence."""
import gzip,json,re,sys
from pathlib import Path
from runtime_select_862_verify import verify,trace_check,select_site

def main(p,build):
    r=json.loads((p/'results.json').read_text());raw=(p/'serial.log').read_bytes();ref=json.loads((p/'reference.json').read_text());site=r['site'];events=r['observations']['runtime_ticks'];trace=gzip.decompress((p/'cpu-trace.log.gz').read_bytes()).decode('ascii')
    print('New linked symbol:',select_site((build/'BOOTX64.EFI').read_bytes(),build/'kernel.map')['selected_context_rva'])
    old=p/'build/x86_64'
    try:select_site((old/'BOOTX64.EFI').read_bytes(),old/'kernel.map')
    except ValueError:pass
    else:raise AssertionError('S861 binary admitted as S862')
    assert not verify(raw,r['qemu_exit_code'],site,ref)[0]
    # Artificial rows intentionally confined to this test; never written to a capture.
    j=0;out=[]
    for line in raw.splitlines(keepends=True):
        if line.startswith(b'STYLEOS S861T FRAME '):
            e=events[j];v=(j,e['id'],e['pid'],e['dispatch'],0,e['safe'],e['root']);out.append(('STYLEOS S862 SELECT '+' '.join(f'{k}=0x{x:016x}' for k,x in zip(('step','id','pid','dispatch','cursor','safe','root'),v))+'\n').encode());j+=1
        out.append(line)
    assert j==10
    raw2=b''.join(out);ok,o=verify(raw2,r['qemu_exit_code'],site,ref);assert ok,o;assert len(o['selected_contexts'])==10;assert json.loads(json.dumps(trace_check(trace,o)))==r['cpu_events']
    n=0
    for h in re.finditer(rb'^STYLEOS S862 SELECT [^\r\n]+',raw2,re.M):
        for field in re.finditer(rb'=0x([0-9a-f]{16})',h[0]):
            a,z=[h.start()+v for v in field.span(1)];old=int(raw2[a:z],16)
            for bit in range(64):
                x=raw2[:a]+f'{old^(1<<bit):016x}'.encode()+raw2[z:];assert not verify(x,r['qemu_exit_code'],site,ref)[0];n+=1
        for x in [raw2[:h.start()]+raw2[h.end():],raw2[:h.start()]+h[0]+b'\n'+raw2[h.start():]]:
            assert not verify(x,r['qemu_exit_code'],site,ref)[0];n+=1
    print('UNIT TEST ONLY: synthetic S862 UART,',n,'refusals PASS; original S861 capture and PE rejected as S862; NOT S862 hardware proof.')
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
