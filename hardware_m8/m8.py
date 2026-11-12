#!/usr/bin/env python3
"""MATRYOSHKA M8: hardware-targeted byte CPU reference simulator and assembler.

3-byte fixed instructions, fetch-low/high/execute state machine, 16-bit address bus.
Pure Python reference only. It does NOT generate a finished Proteus schematic.
"""
import argparse
import ast
from collections import deque
from pathlib import Path
import re

ROM_SIZE=0x8000
RAM_START=0x8000
MEM_SIZE=0x10000
OPERATIONS={
    'NOP':0x00,'LDAI':0x01,'LDBI':0x02,'LDXI':0x03,'LDA_X':0x04,
    'STA_X':0x05,'INX':0x06,'DEX':0x07,'ADDB':0x08,'SUBB':0x09,
    'XORB':0x0A,'CMPB':0x0B,'JMP':0x0C,'JZ':0x0D,'JNZ':0x0E,
    'OUT':0x0F,'IN':0x10,'HALT':0x11,'CMPI':0x12,'ANDB':0x13,
    'ORB':0x14,'INCA':0x15,'DECA':0x16,'MOVAB':0x17,'MOVBA':0x18,
}
OP_TO_NAME={v:k for k,v in OPERATIONS.items()}
IMMEDIATE16={'LDXI','JMP','JZ','JNZ'}
IMMEDIATE8={'LDAI','LDBI','OUT','IN','CMPI'}


def expr(value, labels):
    node=ast.parse(value,mode='eval').body
    def walk(x):
        if isinstance(x,ast.Constant) and type(x.value) is int:return x.value
        if isinstance(x,ast.Name) and x.id in labels:return labels[x.id]
        if isinstance(x,ast.BinOp) and isinstance(x.op,(ast.Add,ast.Sub)):
            return walk(x.left)+walk(x.right) if isinstance(x.op,ast.Add) else walk(x.left)-walk(x.right)
        raise ValueError('Unsupported expression: '+value)
    return walk(node)


def parse(src):
    items=[]
    for lineno,line in enumerate(src.splitlines(),1):
        # ASM source deliberately does not support comment delimiters within strings.
        line=line.split(';',1)[0].strip()
        if line:items.append((lineno,line))
    return items


def assemble(src):
    labels={}
    ins=[]
    pc=0
    for no,line in parse(src):
        if re.match(r'^[A-Za-z_]\w*:',line):
            lbl,line=line.split(':',1)
            if lbl in labels:raise ValueError(f'{no}: duplicate label {lbl}')
            labels[lbl]=pc
            line=line.strip()
            if not line:continue
        if line.startswith('.asciz '):
            literal=ast.literal_eval(line[7:].strip())
            if not isinstance(literal,str):raise ValueError('Expected string')
            raw=literal.encode('ascii')+b'\x00'
            ins.append((no,pc,line))
            pc+=len(raw)
        elif line.startswith('.byte '):
            ins.append((no,pc,line))
            pc+=1
        else:
            ins.append((no,pc,line))
            pc+=3
    if pc>ROM_SIZE:raise ValueError('ROM overflow')
    output=bytearray()
    for no,addr,line in ins:
        try:
            if line.startswith('.asciz '):
                output+=ast.literal_eval(line[7:].strip()).encode('ascii')+b'\0'
                continue
            if line.startswith('.byte '):
                val=expr(line[6:].strip(),labels)
                if not 0<=val<=255:raise ValueError('byte out of range')
                output.append(val)
                continue
            name,*rest=line.split(None,1)
            name=name.upper()
            if name not in OPERATIONS:raise ValueError('Unknown opcode '+name)
            args=rest[0].strip() if rest else ''
            if name in IMMEDIATE8|IMMEDIATE16:
                if not args:raise ValueError('Missing immediate')
                n=expr(args,labels)
                maxv=65535 if name in IMMEDIATE16 else 255
                if not 0<=n<=maxv:raise ValueError('Immediate out of range')
            else:
                if args:raise ValueError('This opcode takes no operands')
                n=0
            output+=bytes([OPERATIONS[name], n&255,(n>>8)&255])
        except (SyntaxError,ValueError) as exc:
            raise ValueError(f'ASM line {no} @ {addr:04X}: {exc}') from exc
    if len(output)!=pc:raise AssertionError('Assembler pass mismatch')
    return bytes(output),labels


def ihex(data):
    result=[]
    for offset in range(0,len(data),16):
        chunk=data[offset:offset+16]
        record=bytes([len(chunk),(offset>>8)&255,offset&255,0])+chunk
        check=(-sum(record))&255
        result.append(':'+ (record+bytes([check])).hex().upper())
    result.append(':00000001FF')
    return '\n'.join(result)+'\n'


def parse_ihex(s):
    mem=bytearray(ROM_SIZE)
    end=0
    terminated=False
    for line in s.splitlines():
        if not line.startswith(':'):raise ValueError('IHEX missing start colon')
        rec=bytes.fromhex(line[1:])
        if len(rec)!=5+rec[0] or sum(rec)&255:raise ValueError('IHEX bad checksum/length')
        count=rec[0]; addr=rec[1]*256+rec[2];rtype=rec[3]
        if rtype==1:
            terminated=True;break
        if rtype!=0 or addr+count>ROM_SIZE:raise ValueError('Unexpected record')
        mem[addr:addr+count]=rec[4:4+count];end=max(end,addr+count)
    if not terminated:raise ValueError('Missing end record')
    return bytes(mem[:end])


class M8:
    def __init__(self,rom,keyboard=b''):
        if len(rom)>ROM_SIZE:raise ValueError('ROM too big')
        self.mem=bytearray(MEM_SIZE)
        self.mem[:len(rom)]=rom
        self.rom=bytes(rom)
        self.pc=0
        self.x=0
        self.a=0
        self.b=0
        self.zero=False
        self.carry=False
        self.phase=0
        self.op=0;self.lo=0;self.hi=0
        self.clocks=0;self.instructions=0
        self.halted=False
        self.keyboard=deque(keyboard)
        self.output=bytearray()
        self.bus_log=[]
    def feed(self,text):
        self.keyboard.extend(text.encode('ascii') if isinstance(text,str) else text)
    def read(self,addr):
        if not 0<=addr<MEM_SIZE:raise ValueError('Bad address')
        return self.mem[addr]
    def clock(self):
        if self.halted:return
        if self.clocks>2_000_000:raise RuntimeError('Clock watchdog')
        self.clocks+=1
        if self.phase==0:
            self.op=self.read(self.pc);self.bus_log.append(('FETCH_OP',self.pc,self.op))
            self.pc=(self.pc+1)&65535
        elif self.phase==1:
            self.lo=self.read(self.pc);self.pc=(self.pc+1)&65535
        elif self.phase==2:
            self.hi=self.read(self.pc);self.pc=(self.pc+1)&65535
        else:
            self.execute()
            self.instructions+=1
        self.phase=(self.phase+1)&3
    def execute(self):
        name=OP_TO_NAME.get(self.op)
        if name is None:raise RuntimeError(f'Illegal opcode {self.op:02X} at {(self.pc-3)&65535:04X}')
        n=self.lo|(self.hi<<8)
        if name=='NOP':pass
        elif name=='LDAI':self.a=self.lo;self.zero=(self.a==0)
        elif name=='LDBI':self.b=self.lo;self.zero=(self.b==0)
        elif name=='LDXI':self.x=n
        elif name=='LDA_X':self.a=self.read(self.x);self.zero=(self.a==0)
        elif name=='STA_X':
            if self.x<0x8000:raise RuntimeError('Attempt to write ROM')
            self.mem[self.x]=self.a
        elif name=='INX':self.x=(self.x+1)&65535
        elif name=='DEX':self.x=(self.x-1)&65535
        elif name=='ADDB':
            z=self.a+self.b;self.carry=z>255;self.a=z&255;self.zero=(self.a==0)
        elif name=='SUBB':
            z=self.a-self.b;self.carry=z<0;self.a=z&255;self.zero=(self.a==0)
        elif name=='XORB':self.a^=self.b;self.zero=(self.a==0)
        elif name=='ANDB':self.a&=self.b;self.zero=(self.a==0)
        elif name=='ORB':self.a|=self.b;self.zero=(self.a==0)
        elif name=='CMPB':self.zero=(self.a==self.b);self.carry=(self.a<self.b)
        elif name=='CMPI':self.zero=(self.a==self.lo);self.carry=(self.a<self.lo)
        elif name=='JMP':self.pc=n
        elif name=='JZ':
            if self.zero:self.pc=n
        elif name=='JNZ':
            if not self.zero:self.pc=n
        elif name=='IN':
            if self.lo==1:self.a=self.keyboard.popleft() if self.keyboard else 0
            else:raise RuntimeError(f'No input port {self.lo}')
            self.zero=(self.a==0)
        elif name=='OUT':
            if self.lo==0:self.output.append(self.a)
            else:raise RuntimeError(f'No output port {self.lo}')
        elif name=='INCA':self.a=(self.a+1)&255;self.zero=(self.a==0)
        elif name=='DECA':self.a=(self.a-1)&255;self.zero=(self.a==0)
        elif name=='MOVAB':self.a=self.b;self.zero=(self.a==0)
        elif name=='MOVBA':self.b=self.a
        elif name=='HALT':self.halted=True
    def run(self,instructions=50000):
        end=self.instructions+instructions
        while not self.halted and self.instructions<end:self.clock()
        return self.instructions
    @property
    def text(self):return self.output.decode('ascii','replace')


def selftest(folder):
    src=(folder/'boot.m8asm').read_text()
    image,labels=assemble(src)
    (folder/'boot.bin').write_bytes(image)
    (folder/'boot.hex').write_text(ihex(image))
    assert parse_ihex(ihex(image))==image
    m=M8(image)
    m.run(500)
    assert 'M8 ROM MONITOR' in m.text and 'M8> ' in m.text,m.text
    m.feed('hmpq')
    m.run(4000)
    assert m.halted,m.text
    assert 'help: h=help' in m.text,m.text
    assert 'RAM TEST OK' in m.text,m.text
    assert 'M8 SHUTDOWN' in m.text,m.text
    assert m.mem[0x8000]==0x5A
    assert len(m.bus_log)>50
    # Clock-by-clock compare with instruction dispatch and ROM read only.
    code,_=assemble('LDAI 255\nLDBI 2\nADDB\nLDXI 0x8000\nSTA_X\nLDAI 0\nLDA_X\nHALT')
    z=M8(code);z.run(100)
    assert z.halted and z.a==1 and z.mem[0x8000]==1 and z.instructions==8
    try:
        bad=M8(assemble('LDAI 42\nLDXI 0\nSTA_X')[0]);bad.run(10)
    except RuntimeError as e:assert 'ROM' in str(e)
    else:raise AssertionError('ROM write protection failed')
    print('M8 SELFTEST PASS | ROM',len(image),'bytes |',len(OPERATIONS),'instructions | boot',m.instructions,'instructions')
    print('Bootstrap and keyboard h/m/p/q, RAM and Intel HEX checksums: PASS')
    print('Boot output tail:',repr(m.text[-100:]))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--selftest',action='store_true')
    parser.add_argument('--build',action='store_true')
    parser.add_argument('--run',action='store_true')
    args=parser.parse_args()
    folder=Path(__file__).resolve().parent
    if args.selftest:selftest(folder);return
    image,labels=assemble((folder/'boot.m8asm').read_text())
    (folder/'boot.bin').write_bytes(image)
    (folder/'boot.hex').write_text(ihex(image))
    print(f'Built ROM {len(image)} bytes / {len(OPERATIONS)} opcodes / {len(labels)} symbols')
    if args.run:
        cpu=M8(image)
        cpu.run(500)
        print(cpu.text,end='')
        while not cpu.halted:
            try:line=input()
            except (EOFError,KeyboardInterrupt):line='q'
            cpu.feed(line)
            previous=len(cpu.output)
            cpu.run(5000)
            print(cpu.output[previous:].decode('ascii','replace'),end='',flush=True)

if __name__=='__main__':main()
