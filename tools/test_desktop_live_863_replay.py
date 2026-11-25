#!/usr/bin/env python3
"""Independently replay the original integrated guest evidence, no synthesized rows."""
import gzip,hashlib,json,sys
from pathlib import Path
from runtime_live_863_verify import site_check,verify,trace_check,frames_check,refusals

def main(out):
    r=json.loads((out/'results.json').read_text());assert r['status']=='PASS' and r['diagnostic_only'] is True and r['exit']==33 and r['live_shell'] is True
    ref=json.loads((out/'reference.json').read_text());efi=(out/'build/x86_64/BOOTX64.EFI').read_bytes();mp=out/'build/x86_64/kernel.map';raw=(out/'serial.log').read_bytes();trace=gzip.decompress((out/'cpu-trace.log.gz').read_bytes())
    for k,v in [('efi',efi),('map',mp.read_bytes()),('uart',raw),('trace',trace)]:assert hashlib.sha256(v).hexdigest()==r[k+'_sha256'],k
    site=site_check(efi,mp);assert site==r['site']==json.loads((out/'site.json').read_text());assert ref==r['reference']
    frames,frame_fnv=frames_check(out);assert frames==r['frames'] and frame_fnv==r['frame_fnv']
    ok,obs=verify(raw,r['exit'],site,ref,frame_fnv);assert ok,obs;assert obs==r['observations']
    events=trace_check(trace.decode('ascii'),obs);assert len(events)==25 and json.loads(json.dumps(events))==r['cpu_events']
    n=refusals(raw,33,site,ref,frame_fnv);assert n==r['mutation_refusals']==r['mutation_cases']
    (out/'independent-replay.json').write_text(json.dumps({'status':'PASS','real_cpu_events':len(events),'screenshots':frames,'uart_refusals':n},indent=2)+'\n')
    print('S863L independent live shell replay PASS:',len(events),'real CPU events,',n,'UART refusals, seven original frames')
if __name__=='__main__':main(Path(sys.argv[1]))
