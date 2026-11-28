"""Build a standalone canonical .tea from a minimal interactive native app.
The ordinary boot image contains approved package bytes, not linked app code."""
import hashlib,struct,zipfile
from tea_package import pack,inspect
def make(folder,index,name,color,file_probe_92=False,file_probe_94=False,file_probe_103=False,explorer_probe_121=False):
    code=bytearray()
    labels={};fix=[]
    def emit(b):code.extend(b)
    def mark(name):labels[name]=len(code)
    def jump(op,name):
        emit(op);fix.append((len(code),name));emit(b'\0'*4)
    # Private data page: first rectangle (0,0,160,120,color).
    emit(b'\xbe\x00\x00\x00\x12')
    for off,value in enumerate((0,0,160,120,color)):
        emit(b'\xc7\x46'+bytes([off*4])+struct.pack('<I',value))
    emit(b'\xb8\x03\x01\x00\x00\x31\xdb\x89\xf1\xba\x01\x00\x00\x00\xcd\x80')
    emit(b'\x85\xc0');jump(b'\x0f\x85','fail')
    if file_probe_92 and index==1:
        # Diagnostic grant is in this process's private DATA + 0x200.
        # Preserve normal frame and input-wait behavior. Verify the actual
        # bytes read from the separately provisioned file, not a stub.
        expected=b"StyleOS: paged file read test.\nSecond line.\n"
        def call(op,addr,size):
            emit(b'\xb8'+struct.pack('<I',op)+b'\xb9'+struct.pack('<I',addr)+b'\xba'+struct.pack('<I',size)+b'\xcd\x80')
        def status(n):
            emit(b'\x83\xf8'+bytes([n]));jump(b'\x0f\x85','fail')
        # rbx=granted token; intentionally wrong grant must fail.
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00\x48\x85\xdb');jump(b'\x0f\x84','fail')
        emit(b'\x48\x83\xf3\x01')
        call(0x130,0,64);status(2)
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        call(0x131,0,0);status(2) # read-only grant cannot write
        call(0x130,0,64);status(0)
        emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
        emit(b'\x48\x89\xd3') # request id
        # Boundary crossing outside single DATA page must not consume.
        call(0x132,0x12000ff0,64);status(1)
        mark('file_take')
        call(0x132,0x12000300,64)
        emit(b'\x83\xf8\x04');jump(b'\x0f\x84','file_take')
        status(0)
        emit(b'\x48\xb8'+struct.pack('<Q',len(expected)<<32)+b'\x48\x39\xc2');jump(b'\x0f\x85','fail')
        for i,value in enumerate(expected):
            emit(b'\x80\xbe'+struct.pack('<I',0x300+i)+bytes([value]));jump(b'\x0f\x85','fail')
        # Completion is one-shot; duplicate take must be denied.
        call(0x132,0x12000300,64);status(2)
        # Also prove the atomic submit/park/resume ABI on the same file.
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        call(0x135,0,64);status(0)
        emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
        emit(b'\x48\x89\xd3')
        for i in range(len(expected)):
            emit(b'\xc6\x86'+struct.pack('<I',0x300+i)+b'\x00')
        call(0x132,0x12000300,64);status(0)
        emit(b'\x48\xb8'+struct.pack('<Q',len(expected)<<32)+b'\x48\x39\xc2');jump(b'\x0f\x85','fail')
        for i,value in enumerate(expected):
            emit(b'\x80\xbe'+struct.pack('<I',0x300+i)+bytes([value]));jump(b'\x0f\x85','fail')
        # 114: real directory grant to fixture root, separate from file grant.
        # Every completion has to match the exact wire row, not just status.
        def ex_submit(op,ordinal):
            emit(b'\xbb'+struct.pack('<I',ordinal))
            call(op,0,0);status(0)
            emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
            emit(b'\x48\x89\xd3')
        def ex_take(label,result,row=False):
            mark(label);call(0x143,0x12000600 if row else 0,296 if row else 0)
            emit(b'\x83\xf8\x04');jump(b'\x0f\x84',label);status(result)
        def ex_row(label,ordinal,kind,object_id,size,revision,name,bad_span=False):
            ex_submit(0x140,ordinal)
            if bad_span:call(0x143,0x12000ff0,296);status(1)
            ex_take(label,0,True)
            expected_wire=struct.pack('<IIIIQQQ',1,kind,len(name),0,object_id,size,revision)+name
            expected_wire=expected_wire.ljust(296,b'\0')
            # Compare a fixed header/name with immediate byte checks; all
            # remaining bytes are zero (repe scasb on the private DATA).
            # Compare whole 32-bit words including zero name padding; no
            # skipped wire bytes. The rest is checked by repe scasb.
            end=(40+len(name)+3)&~3
            for i in range(0,end,4):
                emit(b'\x81\xbe'+struct.pack('<I',0x600+i)+expected_wire[i:i+4])
                jump(b'\x0f\x85','fail')
            emit(b'\xbf'+struct.pack('<I',0x12000600+end)+b'\xb9'+struct.pack('<I',296-end)+b'\x31\xc0\xfc\xf3\xae')
            jump(b'\x0f\x85','fail')
            call(0x143,0x12000600,296);status(2)
        ex_submit(0x142,0);ex_take('ex_root_up',2)
        ex_row('ex_dir',1,2,3,0,0,b'd',True)
        ex_submit(0x141,1);ex_take('ex_enter',0)
        ex_row('ex_file',0,1,4,len(expected),1,b'x.txt')
        ex_submit(0x140,1);ex_take('ex_end',10,True)
        ex_submit(0x141,0);ex_take('ex_file_enter',2)
        ex_submit(0x142,0);ex_take('ex_up',0)
        # 117: the actual mounted fixture is a 512-sector visible block
        # span, NOT the physical 4 TiB disk. Verify exact geometry and
        # positive allocator capacity, plus wrong-size and END receipts.
        ex_submit(0x144,0)
        call(0x143,0x12000600,296);status(1)
        mark('ex_volume');call(0x143,0x12000600,64)
        emit(b'\x83\xf8\x04');jump(b'\x0f\x84','ex_volume');status(0)
        for off,val in ((0,1),(4,0),(24,262144),(28,0),(32,262144),(36,0),(44,0),(52,0),(56,0),(60,0)):
            emit(b'\x81\xbe'+struct.pack('<I',0x600+off)+struct.pack('<I',val));jump(b'\x0f\x85','fail')
        emit(b'\x48\x8b\x86\x28\x06\x00\x00\x48\x85\xc0');jump(b'\x0f\x84','fail')
        emit(b'\x48\x3d\x00\x00\x04\x00');jump(b'\x0f\x87','fail')
        call(0x143,0x12000600,64);status(2)
        ex_submit(0x144,1)
        mark('ex_volume_end');call(0x143,0x12000600,64)
        emit(b'\x83\xf8\x04');jump(b'\x0f\x84','ex_volume_end');status(10)
        # Distinct pixel call serves as evidence only after all byte, directory and volume checks.
        emit(b'\x31\xdb');call(0x100,0x976332,0);status(0)
    if file_probe_94 and index==1:
        payload=b"aStyleOS: paged file read test.\nSecond line.\n"
        def call94(op,addr,size):
            emit(b'\xb8'+struct.pack('<I',op)+b'\xb9'+struct.pack('<I',addr)+b'\xba'+struct.pack('<I',size)+b'\xcd\x80')
        def status94(n):
            emit(b'\x83\xf8'+bytes([n]));jump(b'\x0f\x85','fail')
        # RECEIVE is one-shot; the supervisor, not client, decides rights.
        emit(b'\x31\xdb');call94(0x134,0,0);status94(0)
        emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
        emit(b'\x48\x89\x96\x00\x02\x00\x00')
        call94(0x134,0,0);status94(10)
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        call94(0x131,0x12000ff0,len(payload));status94(1)
        for i,value in enumerate(payload):
            emit(b'\xc6\x86'+struct.pack('<I',0x300+i)+bytes([value]))
        call94(0x136,0x12000300,len(payload));status94(0)
        emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
        emit(b'\x48\x89\xd3')
        mark('save_take')
        call94(0x132,0,0)
        emit(b'\x83\xf8\x04');jump(b'\x0f\x84','save_take')
        status94(0)
        emit(b'\x48\x85\xd2');jump(b'\x0f\x85','fail')
        call94(0x132,0,0);status94(2)
        # Old revision capability may not silently authorize another save.
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        call94(0x131,0x12000300,len(payload));status94(2)
        emit(b'\x31\xdb');call94(0x100,0x976332,0);status94(0)
    if file_probe_103 and index==1:
        # A nonzero word in the second initialized DATA page must survive
        # loading a package larger than the original 16 KiB bound.
        emit(b'\x81\xbe'+struct.pack('<I',0x1ff8)+struct.pack('<I',0x105abcde))
        jump(b'\x0f\x85','fail')
        total=8193;packet=0x12000800
        def call103(op,addr=0,size=0):
            emit(b'\xb8'+struct.pack('<I',op)+b'\xb9'+struct.pack('<I',addr)+b'\xba'+struct.pack('<I',size)+b'\xcd\x80')
        def status103(n):
            emit(b'\x83\xf8'+bytes([n]));jump(b'\x0f\x85','fail')
        def poll103(label,op,addr=0,size=0):
            mark(label);call103(op,addr,size)
            emit(b'\x83\xf8\x04');jump(b'\x0f\x84',label);status103(0)
        def offset103(at):
            emit(b'\x48\xc7\x86'+struct.pack('<I',0x800)+struct.pack('<I',at))
        def expect_count(n):
            emit(b'\x48\x81\xfa'+struct.pack('<I',n));jump(b'\x0f\x85','fail')
        emit(b'\x31\xdb');call103(0x134);status103(0)
        emit(b'\x48\x89\x96\x00\x02\x00\x00\x48\x89\xd3')
        call103(0x137);status103(0)
        emit(b'\x48\x89\xd3')
        # Reject a packet outside the two DATA pages without consumption.
        call103(0x139,0x12001ff0,4104);status103(1)
        for at in (0,4096,8192):
            n=min(4096,total-at)
            offset103(at)
            poll103('read'+str(at),0x139,packet,8+n);expect_count(n)
            emit(b'\xbf'+struct.pack('<I',packet+8)+b'\xb9'+struct.pack('<I',n)+b'\xb8\x41\x00\x00\x00\xfc\xf3\xae')
            jump(b'\x0f\x85','fail')
        call103(0x139,packet,9);status103(10)
        poll103('read_result',0x13e);expect_count(0)
        call103(0x13e);status103(2)
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        call103(0x138,0,total);status103(0)
        emit(b'\x48\x89\xd3')
        for at in (0,4096,8192):
            n=min(4096,total-at)
            offset103(at)
            emit(b'\xbf'+struct.pack('<I',packet+8)+b'\xb9'+struct.pack('<I',n)+b'\xb8\x42\x00\x00\x00\xfc\xf3\xaa')
            poll103('write'+str(at),0x13a,packet,8+n)
        poll103('finish_stream',0x13b)
        poll103('write_result',0x13f);expect_count(0)
        call103(0x13f);status103(2)
        emit(b'\x48\x8b\x9e\x00\x02\x00\x00')
        # Reconciliation has removed the old grant before user resumes.
        call103(0x137);status103(2)
        emit(b'\x31\xdb');call103(0x100,0x976332);status103(0)
    if explorer_probe_121 and index==1:
        # Exact separate ID-managed fixture; scope is directory 3.
        # Private DATA: command 0x400, receipt 0x600, row 0x700.
        def call121(op,addr=0,size=0,arg=None):
            if arg is not None:emit(b'\xbb'+struct.pack('<I',arg))
            emit(b'\xb8'+struct.pack('<I',op)+b'\xb9'+struct.pack('<I',addr)+b'\xba'+struct.pack('<I',size)+b'\xcd\x80')
        def status121(n):
            emit(b'\x83\xf8'+bytes([n]));jump(b'\x0f\x85','fail')
        def take121(label,addr,size):
            emit(b'\x48\x85\xd2');jump(b'\x0f\x84','fail')
            emit(b'\x48\x89\xd3')
            mark(label);call121(0x143,addr,size)
            emit(b'\x83\xf8\x04');jump(b'\x0f\x84',label);status121(0)
        def word121(off,val):
            emit(b'\xc7\x86'+struct.pack('<I',off)+struct.pack('<I',val))
        def expect121(off,val):
            emit(b'\x81\xbe'+struct.pack('<I',off)+struct.pack('<I',val));jump(b'\x0f\x85','fail')
        def copy121(src,dst):
            emit(b'\x48\x8b\x86'+struct.pack('<I',src)+b'\x48\x89\x86'+struct.pack('<I',dst))
        def command121(op,name=b'',ordinal=0,object_id=0):
            emit(b'\xbf\x00\x04\x00\x12\xb9\x40\x01\x00\x00\x31\xc0\xfc\xf3\xaa')
            word121(0x400,1);word121(0x404,op);word121(0x40c,len(name))
            for i in range(3):copy121(0x600+8*i,0x410+8*i)
            word121(0x428,ordinal);word121(0x430,object_id)
            if object_id:copy121(0x720,0x438)
            if name:word121(0x440,int.from_bytes(name.ljust(4,b'\0'),'little'))
        def edit121(label):
            call121(0x145,0x12000400,320,0);status121(0)
            take121(label,0x12000600,24)
            expect121(0x610,3)
        def row121(label,object_id,name):
            call121(0x140,arg=1);status121(0);take121(label,0x12000700,296)
            for off,val in ((0,1),(4,1),(8,1),(16,object_id),(20,0),(24,0),(28,0),(40,ord(name))):
                expect121(0x700+off,val)
        call121(0x146,arg=0);status121(0);take121('ex121_refresh',0x12000600,24)
        expect121(0x610,3)
        command121(1,b'z')
        call121(0x145,0x12000ff0,320,0);status121(1)
        edit121('ex121_create')
        # Original command's snapshot is stale; do not replay mutation.
        call121(0x145,0x12000400,320,0);status121(3)
        row121('ex121_row_z',7,'z')
        command121(3,b'y',1,7);edit121('ex121_rename')
        row121('ex121_row_y',7,'y')
        command121(4,b'',1,7);edit121('ex121_delete')
        command121(1,b'z');edit121('ex121_recreate')
        row121('ex121_row_new',8,'z')
        emit(b'\x31\xdb');call121(0x100,0x976332);status121(0)
    mark('poll')
    emit(b'\xb8\x02\x01\x00\x00\x31\xdb\xb9\x00\x01\x00\x12\xba\x40\x00\x00\x00\xcd\x80')
    emit(b'\x85\xc0');jump(b'\x0f\x85','poll')
    emit(b'\xbf\x00\x01\x00\x12')
    emit(b'\x83\x7f\x0c\x01');jump(b'\x0f\x85','poll') # key event
    emit(b'\x83\x7f\x10\x10');jump(b'\x0f\x85','poll') # Q
    emit(b'\xb8\x02\x00\x00\x00\xcd\x80')
    mark('fail');emit(b'\x0f\x0b')
    for off,target in fix:struct.pack_into('<i',code,off,labels[target]-(off+4))
    wide=file_probe_103 and index==1
    assert len(code)<=4096, f'{name}: code {len(code)} exceeds single-page limit'
    blob=bytearray(4096)+code
    if wide:
        blob.extend(bytes(12288-len(blob)))
        blob.extend(bytes(8192))
        struct.pack_into('<I',blob,12288+0x1ff8,0x105abcde)
    blob[:7]=b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,0x10000000,64,0,0,64,56,2 if wide else 1,0,0,0)
    struct.pack_into('<IIQQQQQQ',blob,64,1,5,4096,0x10000000,0,len(code),4096,4096)
    if wide:struct.pack_into('<IIQQQQQQ',blob,120,1,6,12288,0x12000000,0,8192,8192,4096)
    elf=folder/(name+'.elf');elf.write_bytes(blob)
    tea=folder/(name+'.tea')
    if tea.exists():tea.unlink()
    manifest=pack(tea,elf,'styleos.native_sample_'+str(index),name,'0.1.0','x86_64')
    assert inspect(tea)==manifest
    package=tea.read_bytes()
    with zipfile.ZipFile(tea) as z:meta=z.read('manifest.json')
    assert (16384<len(package)<=136*1024) if wide else len(package)<=16384
    def arr(name,data):return 'static const uint8_t '+name+'[]={'+','.join(str(x) for x in data)+'};\n'
    prefix='sample69_'+str(index)+'_'
    # Peer is stored on a separately provisioned data disk; compile only
    # its independently approved pins and extent, never its package bytes.
    return ((arr(prefix+'package',package) if index==0 else '')+
        '#define '+prefix+'bytes '+str(len(package))+'u\n'+arr(prefix+'manifest',meta)+
        arr(prefix+'pkg_hash',hashlib.sha256(package).digest())+
        arr(prefix+'elf_hash',hashlib.sha256(blob).digest()))
def build(folder,file_probe_92=False,file_probe_94=False,file_probe_103=False,explorer_probe_121=False):
    (folder/'native_sample_69.h').write_text(
        make(folder,0,'Native-sample',0x2579a1)+make(folder,1,'Native-peer',0x976332,file_probe_92=file_probe_92,file_probe_94=file_probe_94,file_probe_103=file_probe_103,explorer_probe_121=explorer_probe_121))
    # Both approved packages must carry independent executable identities.
    first=(folder/'Native-sample.elf').read_bytes()
    second=(folder/'Native-peer.elf').read_bytes()
    if first==second or hashlib.sha256(first).digest()==hashlib.sha256(second).digest():
        raise ValueError('native sample ELF identity collision')
