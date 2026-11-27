"""Build the ordinary trusted shell controller as a separate immutable ELF.
No diagnostic code, epoch-specific faults or arbitrary external disk reader."""
import hashlib,struct
def build(folder):
    code=bytearray()
    def emit(b):code.extend(b)
    fix=[]
    def reject(op):
        emit(bytes([op,0]));fix.append(len(code)-1)
    emit(b'\xb8\x05\x01\x00\x00\xcd\x80') # read event
    emit(b'\x48\x85\xc0');reject(0x74)
    emit(b'\x48\x89\xc3\x48\xc1\xeb\x20') # epoch
    # The controller selects policy from key -> action, not the renderer.
    # Default refusal; each branch explicitly selects an action.
    emit(b'\x83\xf8\x44\x74\x10\x83\xf8\x3e\x74\x12\x83\xf8\x42');reject(0x75)
    emit(b'\xb9\x03\x00\x00\x00\xeb\x0c')
    emit(b'\xb9\x01\x00\x00\x00\xeb\x05')
    emit(b'\xb9\x02\x00\x00\x00')
    emit(b'\xb8\x06\x01\x00\x00\xcd\x80')
    emit(b'\x48\x39\xd8');reject(0x75) # exact acknowledged epoch
    emit(b'\xb8\x02\x00\x00\x00\xcd\x80') # clean exit
    fail=len(code);emit(b'\x0f\x0b')
    for off in fix:
        delta=fail-(off+1)
        assert 0<=delta<=127
        code[off]=delta
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
