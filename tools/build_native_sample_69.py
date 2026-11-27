"""Build a standalone canonical .tea from a minimal interactive native app.
The ordinary boot image contains approved package bytes, not linked app code."""
import hashlib,struct,zipfile
from tea_package import pack,inspect
def make(folder,index,name,color):
    code=bytearray()
    labels={};fix=[]
    def emit(b):code.extend(b)
    def mark(name):labels[name]=len(code)
    def jump(op,name):
        emit(op);fix.append((len(code),name));emit(b'\0'*4)
    # Private data page: first rectangle (0,0,160,120,color).
    emit(b'\xbe\x00\x00\x00\x12')
    for off,value in enumerate((0,0,160,120,0x2579a1)):
        emit(b'\xc7\x46'+bytes([off*4])+struct.pack('<I',value))
    emit(b'\xb8\x03\x01\x00\x00\x31\xdb\x89\xf1\xba\x01\x00\x00\x00\xcd\x80')
    emit(b'\x85\xc0');jump(b'\x0f\x85','fail')
    mark('poll')
    emit(b'\xb8\x02\x01\x00\x00\x31\xdb\xb9\x00\x01\x00\x12\xba\x40\x00\x00\x00\xcd\x80')
    emit(b'\x85\xc0');jump(b'\x0f\x85','poll')
    emit(b'\xbf\x00\x01\x00\x12')
    emit(b'\x83\x7f\x0c\x01');jump(b'\x0f\x85','poll') # key event
    emit(b'\x83\x7f\x10\x10');jump(b'\x0f\x85','poll') # Q
    emit(b'\xb8\x02\x00\x00\x00\xcd\x80')
    mark('fail');emit(b'\x0f\x0b')
    for off,name in fix:struct.pack_into('<i',code,off,labels[name]-(off+4))
    blob=bytearray(4096)+code
    blob[:7]=b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH',blob,16,2,62,1,0x10000000,64,0,0,64,56,1,0,0,0)
    struct.pack_into('<IIQQQQQQ',blob,64,1,5,4096,0x10000000,0,len(code),4096,4096)
    elf=folder/(name+'.elf');elf.write_bytes(blob)
    tea=folder/(name+'.tea')
    if tea.exists():tea.unlink()
    manifest=pack(tea,elf,'styleos.native_sample_'+str(index),name,'0.1.0','x86_64')
    assert inspect(tea)==manifest
    package=tea.read_bytes()
    with zipfile.ZipFile(tea) as z:meta=z.read('manifest.json')
    assert len(package)<=16384
    def arr(name,data):return 'static const uint8_t '+name+'[]={'+','.join(str(x) for x in data)+'};\n'
    prefix='sample69_'+str(index)+'_'
    return (arr(prefix+'package',package)+arr(prefix+'manifest',meta)+
        arr(prefix+'pkg_hash',hashlib.sha256(package).digest())+
        arr(prefix+'elf_hash',hashlib.sha256(blob).digest()))
def build(folder):
    (folder/'native_sample_69.h').write_text(
        make(folder,0,'Native-sample',0x2579a1)+make(folder,1,'Native-peer',0x976332))
