"""Build CI workload separately from the kernel. No test code bytes in EFI."""
import hashlib,json,struct,subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CODE=0x10000000
def read_object(blob):
    if blob[:7]!=b'\x7fELF\x02\x01\x01' or struct.unpack_from('<HH',blob,16)!=(1,62):
        raise ValueError('external workload ELF64 relocatable')
    off=struct.unpack_from('<Q',blob,40)[0]
    size,num=struct.unpack_from('<HH',blob,58)
    if size!=64 or not num or off+size*num>len(blob):raise ValueError('section bounds')
    sections=[struct.unpack_from('<IIQQQQIIQQ',blob,off+64*i) for i in range(num)]
    def data(s):
        a,n=s[4:6]
        if a+n>len(blob):raise ValueError('section extent')
        return blob[a:a+n]
    symbols={}
    for s in sections:
        if s[1]!=2:continue
        if s[6]>=num or s[9]!=24 or s[5]%24:raise ValueError('symbols')
        st=data(sections[s[6]])
        b=data(s)
        for i in range(0,len(b),24):
            name,_,_,idx,value,_=struct.unpack_from('<IBBHQQ',b,i)
            if name>=len(st):raise ValueError('symbol name')
            key=st[name:].split(b'\0',1)[0].decode('ascii')
            if key.startswith('s848_'):
                if key in symbols or not 0<idx<num:raise ValueError('symbol provenance')
                symbols[key]=(idx,value)
    results=[]
    for i,label in enumerate('abcd'):
        prefix='s848_'+label+'_'
        names=['start','yield','loop_end','exit' if label=='b' else 'fault','end']
        loc=[symbols[prefix+n] for n in names]
        idx=loc[0][0]
        if any(k!=idx for k,v in loc) or not sections[idx][2]&4:raise ValueError('text binding')
        if any(s[1] in (4,9) and s[7]==idx and s[5] for s in sections):raise ValueError('relocated test code')
        start,call,loop,terminal,end=[v for k,v in loc]
        code=data(sections[idx])[start:end]
        if not start<call<loop<=terminal<end or len(code)!=end-start or not 0<len(code)<=4096:raise ValueError('code extent')
        elf=bytearray(4096+len(code));elf[:7]=b'\x7fELF\x02\x01\x01'
        struct.pack_into('<HHIQQQIHHHHHH',elf,16,2,62,1,CODE,64,0,0,64,56,1,0,0,0)
        struct.pack_into('<IIQQQQQQ',elf,64,1,5,4096,CODE,CODE,len(code),4096,4096)
        elf[4096:]=code
        results.append((bytes(elf),{'id':i,'bytes':len(elf),'code_bytes':len(code),
            'yield':CODE+call-start,'end':CODE+loop-start,'term':CODE+terminal-start,
            'sha256':hashlib.sha256(elf).hexdigest(),
            'page_sha256':hashlib.sha256(code.ljust(4096,b'\0')).hexdigest()}))
    return results
def build(cc,folder):
    obj=folder/'external-workload-865.o'
    subprocess.run([cc,'--target=x86_64-unknown-linux-gnu','-DSTYLEOS_TEST_USER_PREEMPT_848=1',
        '-DSTYLEOS_TEST_FX_CONTEXT_849=1','-DSTYLEOS_TEST_RUNTIME_MIXED_863=1',
        '-DSTYLEOS_TEST_FOUR_GUEST_864=1','-c',str(ROOT/'tests/guest/workload_865.S'),'-o',str(obj)],check=True)
    entries=read_object(obj.read_bytes())
    rows=[]
    for i,(elf,meta) in enumerate(entries):
        (folder/f'external-workload-{i}.elf').write_bytes(elf)
        digest=lambda key:'{'+','.join('0x%02x'%b for b in bytes.fromhex(meta[key]))+'}'
        rows.append('{%du,%du,0x%xULL,0x%xULL,0x%xULL,%s,%s}'%(meta['bytes'],meta['code_bytes'],meta['yield'],meta['end'],meta['term'],digest('sha256'),digest('page_sha256')))
    (folder/'external-workload-865.json').write_text(json.dumps([m for _,m in entries],indent=2)+'\n')
    (folder/'external_workload_865.h').write_text(
        '/* CI metadata only; no executable bytes. */\n'
        'static const struct { unsigned bytes,code_bytes; uint64_t call,loop,term; uint8_t hash[32],page_hash[32]; } external865[4]={\n'+',\n'.join(rows)+'\n};\n')
