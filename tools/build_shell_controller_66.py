"""Build the ordinary trusted shell controller as a separate immutable ELF.
No diagnostic code, epoch-specific faults or arbitrary external disk reader."""
import hashlib,struct
def build(folder):
    code=bytearray()
    labels={};fixups=[]
    def emit(b):code.extend(b)
    def mark(name):
        assert name not in labels
        labels[name]=len(code)
    def jump(op,target):
        emit(op);fixups.append((len(code),target));emit(b'\0'*4)
    def cmp(value):
        emit(b'\x3d'+struct.pack('<I',value))
    def eq(value,target):
        cmp(value);jump(b'\x0f\x84',target)
    emit(b'\xb8\x05\x01\x00\x00\xcd\x80') # read event
    emit(b'\x48\x85\xc0');jump(b'\x0f\x84','fail')
    emit(b'\x48\x89\xc3\x48\xc1\xeb\x20') # epoch
    eq(0x44,'a1');eq(0x3e,'a2');eq(0x42,'a3')
    eq(0x200,'a6');eq(0x201,'a8')
    cmp(0x101);jump(b'\x0f\x82','fail')
    cmp(0x12a);jump(b'\x0f\x82','a4')
    cmp(0x154);jump(b'\x0f\x82','a5')
    cmp(0x17e);jump(b'\x0f\x82','a6')
    jump(b'\x0f\x84','a7')
    jump(b'\xe9','fail')
    for action in range(1,9):
        mark('a'+str(action))
        emit(b'\xb9'+struct.pack('<I',action))
        jump(b'\xe9','decide')
    mark('decide')
    emit(b'\xb8\x06\x01\x00\x00\xcd\x80')
    emit(b'\x48\x39\xd8');jump(b'\x0f\x85','fail')
    emit(b'\xb8\x02\x00\x00\x00\xcd\x80')
    mark('fail');emit(b'\x0f\x0b')
    for off,target in fixups:
        struct.pack_into('<i',code,off,labels[target]-(off+4))
    assert len(code)<4096
    blob=bytearray(4096)+code
    blob[:7]=b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,0x10000000,64,0,0,64,56,1,0,0,0)
    struct.pack_into('<IIQQQQQQ',blob,64,1,5,4096,0x10000000,0,len(code),4096,4096)
    digest=hashlib.sha256(blob).digest()
    padded=blob.ljust((len(blob)+511)//512*512,b'\0')
    def values(b):return ','.join(str(x) for x in b)
    (folder/'shell_controller_66.h').write_text(
        '/* Generated ordinary system controller; verified by native ELF loader. */\n'
        'static const unsigned char shell66_image[]={'+values(padded)+'};\n'
        'static const unsigned char shell66_hash[32]={'+values(digest)+'};\n'
        '#define SHELL66_BYTES '+str(len(blob))+'u\n')
    (folder/'StyleOS-shell-controller.elf').write_bytes(blob)
