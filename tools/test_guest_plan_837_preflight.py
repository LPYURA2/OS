"""Host-only tests on genuine S835 maps, with synthetic guest live-range records."""
import copy,json,subprocess,sys,zipfile
from contextlib import nullcontext
from pathlib import Path
from iommu_guest_837_verify import check_live_map,subtract,merge
from uefi_map_826 import parse

def tests(evidence):
    count=0
    with (nullcontext(None) if evidence.is_dir() else zipfile.ZipFile(evidence)) as z:
        read=(lambda name:(evidence/name).read_bytes()) if z is None else z.read
        root='acpi-835/qemu/'
        meta=json.loads(read(root+'results.json'))
        for mode,rec in meta['modes'].items():
            if rec['status']=='SKIPPED':continue
            data=read(root+mode+'/final-map.bin');m=rec['observations']['s826_map_audit'];stride=m['descriptor_size'];version=m['descriptor_version']
            rows=parse(data,stride,version)
            ram=[];reserved=[]
            for r in rows:
                attr=r['attributes'];(ram if r['type']==7 and attr&8 and not attr&~0x401f else reserved).append([r['start'],r['end_exclusive']])
            # Synthetic live addresses confined to the existing non-RAM map.
            a,b=next((a,b) for a,b in reserved if b-a>=len(data)+8192 and a>=0x100000)
            live={'IMAGE':[[a,a+4096]],'HANDOFF':[[a,a+96]],'SOURCE':[[a+4096,a+4096+len(data)]]}
            protected=merge(reserved+[[lo&~4095,(hi+4095)&~4095] for spans in live.values() for lo,hi in spans])
            groups=dict(live,RAM=ram,RESERVED=protected,CANDIDATE=subtract(ram,protected))
            def prefix(g):return ''.join(f'STYLEOS MEMMAP {k} 0x{a:016x} 0x{b:016x}\n' for k,spans in g.items() for a,b in spans).encode()
            owner={'source_data':dict(pa=a+4096,bytes=len(data),stride=stride,version=version)}
            check_live_map(prefix(groups),data,stride,version,owner);count+=1
            for key in ('RAM','RESERVED','CANDIDATE','SOURCE'):
                g=copy.deepcopy(groups);g[key][0][1]+=4096
                try:check_live_map(prefix(g),data,stride,version,owner)
                except (ValueError,KeyError):count+=1
                else:raise AssertionError((mode,key))
            for key in ('pa','bytes','stride','version'):
                o=copy.deepcopy(owner);o['source_data'][key]+=1
                try:check_live_map(prefix(groups),data,stride,version,o)
                except ValueError:count+=1
                else:raise AssertionError((mode,key))
    # Refuse incomplete mode combinations before reaching compiler/firmware.
    runner=Path(__file__).with_name('test_acpica_firmware_823.py')
    flags=['--irq-sources-830','--capture-post-ebs-834','--native-quarantine-835']
    for omit in flags:
        args=[sys.executable,str(runner),'--guest-plan-837',*[f for f in flags if f!=omit],'--output','/tmp/invalid-s837','--core-lib','/tmp/not-present.lib','--core-include','/tmp/not-present']
        p=subprocess.run(args,capture_output=True,text=True)
        assert p.returncode==2 and 'S837 requires S835 and paired snapshots' in p.stderr;count+=1
    print(f'{count}/{count} map-provenance and profile-refusal checks PASS (synthetic live records)')
if __name__=='__main__':tests(Path(sys.argv[1]))
