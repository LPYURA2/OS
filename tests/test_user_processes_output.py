"""Adversarial hardware-frame/UART fixtures, distinct from real QEMU evidence."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.test import (Checks, _USER_MACHINE_FIELDS, _USER_FRAME_FIELDS,
    _user_processes_sources, serial_handoff_ok, serial_user_processes_ok,
    user_processes_report, verified_user_processes_site)


def line(label, **fields):
    return ('STYLEOS USER ' + label + ''.join(f' {key}=0x{value:016x}'
            for key, value in fields.items()) + '\n').encode()


def bounds(label, a, b):
    return f'STYLEOS {label} 0x{a:016x} 0x{b:016x}\n'.encode()


def fixture(arch='aarch64', el2=False):
    base = 0x60000000 if arch == 'x86_64' else 0x40080000
    image = base, base + 0x180000
    sizes = dict(tables=(0x20000,393216), control=(0x80000,16384),
        entry_stack=(0x84000,16384), stack_a=(0x88000,4096), stack_b=(0x89000,4096),
        data_a=(0x8a000,4096), data_b=(0x8b000,4096), code_a=(0x2000,4096),
        code_b=(0x3000,4096), diagnostic=(0x5000,12400), stack=(0x90000,65536),
        pool=(0x10000,32768),pool_metadata=(0x18000,4096))
    if arch == 'x86_64':
        sizes.update(idt=(0xa1000,4096),handoff=(0xa0000,96),
                     gdt=(0xb0000,4096),tss=(0xb1000,4096),user_idt=(0xb2000,4096))
        prefix=(b'STYLEOS x86_64 ExitBootServices OK\n'
                b'STYLEOS x86_64 IDT ready cs=0x0000000000000038\n')
        handoff=base+0xa0000,base+0xa0060
        source=base+0x1a0000,base+0x1a1000
        ram=base,base+0x400000
        reserves=[image,source]
    else:
        sizes.update(vectors=(0x1000,2048),user_vectors=(0x1800,2048))
        prefix=(b'STYLEOS AARCH64: initial EL2\n' if el2 else
                b'STYLEOS AARCH64: initial EL1\n')+b'STYLEOS AARCH64: current EL1\n'
        handoff=base+0x98000,base+0x98060
        source=0x44000000,0x44001000
        ram=0x40000000,0x48000000
        reserves=[(0x9000000,0x9001000),image,source]
    objects={key:(base+offset,base+offset+size) for key,(offset,size) in sizes.items()}
    prefix+=(b'STYLEOS kernel_main entered\nSTYLEOS boot_info validated v1\n'+
             f'STYLEOS HANDOFF PASS {arch}\n'.encode()+b'STYLEOS MEMMAP DIAGNOSTIC ONLY\n')
    for kind, spans in [('IMAGE',[image]),('STACK',[objects['stack']]),('HANDOFF',[handoff]),
                        ('SOURCE',[source]),('RAM',[ram]),('RESERVED',reserves)]:
        for a,b in spans:prefix+=bounds('MEMMAP '+kind,a,b)
    cursor=ram[0]
    for a,b in reserves:
        if b<=ram[0] or a>=ram[1]:continue
        if cursor<a:prefix+=bounds('MEMMAP CANDIDATE',cursor,a)
        cursor=max(cursor,b)
    if cursor<ram[1]:prefix+=bounds('MEMMAP CANDIDATE',cursor,ram[1])
    prefix+=b'STYLEOS MEMMAP PASS\n'
    relocation=base if arch=='x86_64' else 0
    site=dict(arch=arch,page_size=4096,image_start=image[0]-relocation,
        image_end=image[1]-relocation,image_size=image[1]-image[0],
        address_space='image-rva' if arch=='x86_64' else 'identity-image-address',
        fault_va=0x12010000,objects={key:dict(start=a-relocation,end=b-relocation,bytes=b-a)
                                    for key,(a,b) in objects.items()},programs={})
    site['integer_only']=dict(compiler_flags=['fixture'],decoder_version='LLVM version 19.1.7',
        decoded_bytes=4,instruction_count=1,executable_sha256=hashlib.sha256(b'fixture').hexdigest())
    for ident in ('A','B'):
        page=bytearray(bytes.fromhex('90')*4096 if arch=='x86_64' else bytes.fromhex('1f2003d5')*1024)
        trap=bytes.fromhex('cd80' if arch=='x86_64' else '210000d4')
        exit_trap=bytes.fromhex('cd80' if arch=='x86_64' else '410000d4')
        yield_offset=0x20 if ident=='A' else 0x40
        fault_offset=0x60
        exit_offset=0x80
        page[yield_offset-len(trap):yield_offset]=trap
        if ident=='A':
            fault=bytes.fromhex('488b07' if arch=='x86_64' else '010040f9')
            page[fault_offset:fault_offset+len(fault)]=fault
        else:page[exit_offset:exit_offset+len(trap)]=exit_trap
        def position(offset,size=1):
            raw=bytes(page[offset:offset+size]);key='image_rva' if arch=='x86_64' else 'image_pc'
            return dict(user_pc=0x10000000+offset,raw_hex=raw.hex(),sha256=hashlib.sha256(raw).hexdigest(),
                        **{key:objects['code_'+ident.lower()][0]+offset-relocation})
        instr=1 if arch=='x86_64' else 4
        sites=dict(entry=position(0,instr),yield_resume=position(yield_offset,instr))
        sites['yield_resume']['call_site']=position(yield_offset-len(trap),len(trap))
        if ident=='A':sites['fault']=position(fault_offset,len(fault))
        else:sites.update(exit=position(exit_offset,len(trap)),exit_resume=position(exit_offset+len(trap),instr))
        site['programs'][ident]=dict(code_object='code_'+ident.lower(),code_va=0x10000000,
            stack_va=0x11000000 if ident=='A' else 0x11010000,
            data_va=0x12000000 if ident=='A' else 0x12010000,
            sites=sites,raw_hex=page.hex(),sha256=hashlib.sha256(page).hexdigest())
    out=prefix
    for name,(a,b) in objects.items():out+=bounds('USER OBJECT name='+name,a,b)
    roots={}
    for root_number,ident in enumerate(('SAFE','A','B')):
        root=objects['tables'][0]+root_number*32*4096
        tables=[{}]
        def map_page(va,pa,user=False,write=True,execute=True,device=False):
            table=0
            for shift in (39,30,21):
                index=va>>shift&511
                if index not in tables[table]:
                    child=len(tables);tables.append({})
                    tables[table][index]=(root+child*4096)|(7 if arch=='x86_64' else 3)
                table=((tables[table][index]&((1<<48)-1)&~4095)-root)//4096
            if arch=='x86_64':flags=1|(2 if write else 0)|(4 if user else 0)|(0 if execute else 1<<63)
            elif device:flags=3|4|(2<<8)|(1<<10)|(3<<53)
            elif user:flags=3|(3<<8)|(1<<10)|(1<<53)|((1<<6)|(1<<54) if write else 3<<6)
            else:flags=3|(3<<8)|(1<<10)|(1<<54)
            tables[table][va>>12&511]=pa|flags
        for va in range(image[0],image[1],4096):map_page(va,va)
        if arch=='aarch64':map_page(0x9000000,0x9000000,execute=False,device=True)
        if ident!='SAFE':
            for kind in ('code','stack','data'):
                map_page(site['programs'][ident][kind+'_va'],objects[kind+'_'+ident.lower()][0],
                         user=True,write=kind!='code',execute=kind=='code')
        roots[ident]=root
        out+=line('ROOT id='+ident,pa=root,pages=len(tables))
        entries=0
        for table,slots in enumerate(tables):
            for index,entry in sorted(slots.items()):
                out+=line('TABLE id='+ident,table=root+table*4096,index=index,entry=entry);entries+=1
        out+=line('TABLES END id='+ident,entries=entries)
    if arch=='x86_64':
        machine=dict(root=roots['SAFE'],mode=0x80010011,control0=0x6a0,control1=0xd00,
                     control2=0x0007040600070406,control3=8,vectors=objects['user_idt'][0],
                     gdt=objects['gdt'][0],tss=0x28)
    else:
        tcr=16|(3<<12)|(16<<16)|(1<<23)|(2<<30)|(2<<32)
        machine=dict(root=roots['SAFE'],mode=0x30d00801,control0=tcr,control1=0x44,
                     control2=4,control3=0x3c0,vectors=objects['user_vectors'][0],gdt=0,tss=0)
    out+=line('MACHINE phase=SAFE',**machine)
    for count,(ident,kind) in enumerate((('A','YIELD'),('B','YIELD'),('A','FAULT'),('B','EXIT')),1):
        program=site['programs'][ident];sp=program['stack_va']+4096
        pc=program['sites']['entry' if count<=2 else 'yield_resume']['user_pc']
        out+=line('GO id='+ident,count=count,root=roots[ident],pc=pc,sp=sp if count<=2 else sp-16)
        saved_pc=program['sites']['yield_resume' if kind=='YIELD' else 'fault' if kind=='FAULT' else 'exit_resume']['user_pc']
        frame=dict(vector=(14 if kind=='FAULT' else 128) if arch=='x86_64' else 8,
            syndrome=(4 if kind=='FAULT' else 0) if arch=='x86_64' else
                     (0x92000007 if kind=='FAULT' else 0x56000001 if kind=='YIELD' else 0x56000002),
            address=0x12010000 if kind=='FAULT' else 0,root=roots[ident],
            kernel_sp=objects['entry_stack'][1]-(176 if arch=='x86_64' else 320),
            pc=saved_pc,sp=sp-16,status=2 if arch=='x86_64' else 0x3c0,
            cs=0x23 if arch=='x86_64' else 0,ss=0x1b if arch=='x86_64' else 0)
        out+=line('FRAME id='+ident+' kind='+kind,**frame)
        registers=[0]*31
        registers[1 if arch=='x86_64' else 19]=0xa11c if ident=='A' else 0xb22c
        registers[11 if arch=='x86_64' else 20]=0xa11ce55a12345678 if ident=='A' else 0x123456789abcdef0
        if ident=='B':registers[9]=0x55aa001122334455
        if arch=='x86_64':
            registers[0]=2 if kind=='EXIT' else 1
            if kind=='FAULT':registers[5]=0x12010000
        else:registers[0]=0x12010000 if kind=='FAULT' else 2 if kind=='EXIT' else 1
        for index,value in enumerate(registers):out+=line('REG id='+ident,index=index,value=value)
        out+=line('DATA id=A',value=0)+line('DATA id=B',value=0 if count==1 else
                0xfedcba9876543210 if count==4 else 0x123456789abcdef0)
        out+=line('SAFE',root=roots['SAFE'],sp=objects['stack'][1]-256)
        if kind=='FAULT':out+=b'STYLEOS USER TERMINATED id=A state=FAULTED\n'
    out+=line('EMPTY',faulted=1,exited=1,dispatches=4)
    for ident in ('A','B'):out+=line('RELEASE id='+ident+' state=FREE',root=roots['SAFE'])
    out+=line('MACHINE phase=FINAL',**machine)+b'STYLEOS USER PASS\nSTYLEOS KERNEL HALT\n'
    return out,site


class UserProcessTraceTests(unittest.TestCase):
    def reject(self,output,site,arch='aarch64'):
        self.assertFalse(serial_user_processes_ok(output,arch,site))
        self.assertIsNone(user_processes_report(output,arch,site))

    def replace_field(self,output,marker,field,value):
        row=next(row for row in output.splitlines(keepends=True) if marker in row)
        old=row.split(field+b'=')[1].split()[0]
        new=row.replace(field+b'='+old,field+b'='+f'0x{value:016x}'.encode())
        return output.replace(row,new,1)

    def test_both_isa_and_arm_el_have_four_hardware_entries(self):
        for arch,el2 in [('x86_64',False),('aarch64',False),('aarch64',True)]:
            output,site=fixture(arch,el2)
            self.assertTrue(serial_user_processes_ok(output,arch,site,el2=el2))
            report=user_processes_report(output,arch,site,el2=el2)
            self.assertEqual([(e['id'],e['kind']) for e in report['schedule']],
                             [('A','YIELD'),('B','YIELD'),('A','FAULT'),('B','EXIT')])
            self.assertEqual(report['faulted'],['A']);self.assertEqual(report['exited'],['B'])
            self.assertEqual(report['released'],['A','B'])
            self.assertEqual(len([e for e in report['roots']['A']['leaves'] if e['user']]),3)

    def test_pass_only_reset_timeout_missing_prefix_and_halt_fail(self):
        output,site=fixture()
        for bad in (b'STYLEOS USER PASS\n',output+b'timeout\n',output+b'timed out\n',
                    output.replace(b'STYLEOS MEMMAP PASS\n',b''),
                    output.replace(b'STYLEOS KERNEL HALT\n',b''),
                    output.replace(b'STYLEOS HANDOFF PASS aarch64\n',b'')):
            self.reject(bad,site)
        self.assertFalse(serial_handoff_ok(output,'aarch64'))

    def test_kernel_origin_wrong_syndrome_pc_va_and_fault_slot_fail(self):
        for arch in ('x86_64','aarch64'):
            output,site=fixture(arch)
            changes=[(b'FRAME id=A kind=FAULT',b'address',0x12000000),
                     (b'FRAME id=A kind=FAULT',b'pc',0x10000064),
                     (b'FRAME id=A kind=FAULT',b'root',0),
                     (b'FRAME id=A kind=FAULT',b'vector',13 if arch=='x86_64' else 4),
                     (b'FRAME id=A kind=FAULT',b'syndrome',5 if arch=='x86_64' else 0x96000007),
                     (b'FRAME id=A kind=FAULT',b'cs' if arch=='x86_64' else b'status',8 if arch=='x86_64' else 0x3c5)]
            for marker,field,value in changes:
                self.reject(self.replace_field(output,marker,field,value),site,arch)
            self.reject(output.replace(b'FRAME id=A kind=FAULT',b'FRAME id=B kind=FAULT'),site,arch)

    def test_b_cannot_exit_before_fault_or_resume_a_again(self):
        output,site=fixture()
        self.reject(output.replace(b'FRAME id=B kind=YIELD',b'FRAME id=B kind=EXIT'),site)
        self.reject(output.replace(b'GO id=B count=0x0000000000000004',b'GO id=A count=0x0000000000000004'),site)
        self.reject(output.replace(b'STYLEOS USER PASS\n',line('GO id=A',count=5,root=0,pc=0,sp=0)+b'STYLEOS USER PASS\n'),site)
        a=next(row for row in output.splitlines(keepends=True) if b'FRAME id=A kind=FAULT' in row)
        b=next(row for row in output.splitlines(keepends=True) if b'FRAME id=B kind=EXIT' in row)
        self.reject(output.replace(a,b'__SWAPPED_FRAME__\n',1).replace(b,a,1)
                    .replace(b'__SWAPPED_FRAME__\n',b,1),site)

    def test_stack_root_register_and_private_continuation_are_bound(self):
        for arch in ('x86_64','aarch64'):
            output,site=fixture(arch)
            for marker,field,value in [(b'FRAME id=B kind=EXIT',b'sp',0x11001000-16),
                (b'FRAME id=B kind=EXIT',b'kernel_sp',0x11011000-320),
                (b'GO id=B count=0x0000000000000004',b'pc',0x10000000),
                (b'GO id=B count=0x0000000000000004',b'sp',0x11011000),
                (b'REG id=B index=0x0000000000000009',b'value',0),
                (b'DATA id=B value=0xfedcba9876543210',b'value',0x123456789abcdef0),
                (b'SAFE root=',b'root',0),
                (b'RELEASE id=A',b'root',0)]:
                self.reject(self.replace_field(output,marker,field,value),site,arch)

    def test_complete_descriptor_set_forbids_user_kernel_foreign_and_wx_aliases(self):
        for arch in ('x86_64','aarch64'):
            output,site=fixture(arch)
            aroot=site['objects']['tables']['start']+(0x60000000 if arch=='x86_64' else 0)+32*4096
            row=next(row for row in output.splitlines(keepends=True)
                     if b'TABLE id=A' in row and b'TABLES' not in row and
                     (int(row.split(b'entry=')[1],16)&~4095&((1<<48)-1))==site['objects']['data_a']['start']+(0x60000000 if arch=='x86_64' else 0) and
                     int(row.split(b'entry=')[1],16)&(4 if arch=='x86_64' else 64))
            old=int(row.split(b'entry=')[1],16)
            foreign=site['objects']['data_b']['start']+(0x60000000 if arch=='x86_64' else 0)
            for value in [(old&~(((1<<48)-1)&~4095))|foreign,
                          old&~(1<<63) if arch=='x86_64' else old&~(1<<54),
                          old&~4 if arch=='x86_64' else old&~(1<<6)]:
                bad=row.split(b'entry=')[0]+f'entry=0x{value:016x}\n'.encode()
                self.reject(output.replace(row,bad,1),site,arch)
            self.reject(output.replace(row,b'',1),site,arch)
            self.reject(output.replace(row,row*2,1),site,arch)
            self.reject(self.replace_field(output,b'ROOT id=A',b'pa',aroot+4096),site,arch)

    def test_privileged_stack_contains_the_complete_aligned_hardware_frame(self):
        for arch in ('x86_64', 'aarch64'):
            output, site = fixture(arch)
            end = site['objects']['entry_stack']['end'] + (0x60000000 if arch == 'x86_64' else 0)
            for sp in (end - 8, end - 16, end - (176 if arch == 'x86_64' else 320) + 1):
                with self.subTest(arch=arch, sp=sp):
                    self.reject(self.replace_field(output, b'FRAME id=B kind=EXIT',
                                                   b'kernel_sp', sp), site, arch)

    def test_missing_repeated_or_unexpected_records_cannot_be_erased_by_pass(self):
        output,site=fixture()
        for marker in (b'TABLES END id=A',b'FRAME id=B kind=EXIT',b'REG id=B index=0x0000000000000009',
                       b'TERMINATED id=A',b'EMPTY faulted=',b'RELEASE id=A',b'MACHINE phase=FINAL'):
            row=next(row for row in output.splitlines(keepends=True) if marker in row)
            self.reject(output.replace(row,b'',1),site)
            self.reject(output.replace(row,row*2,1),site)
        self.reject(output+b'STYLEOS USER PASS\n',site)
        self.reject(output.replace(b'STYLEOS USER PASS\n',b'STYLEOS USER ERROR: kernel\nSTYLEOS USER PASS\n'),site)

    def test_modified_binary_metadata_is_not_trusted_by_parser(self):
        output,site=fixture()
        for mutate in (lambda d:d['programs']['A']['sites']['fault'].update(user_pc=0x10000064),
                       lambda d:d['programs']['B'].update(sha256='0'*64),
                       lambda d:d['objects']['control'].update(bytes=4096),
                       lambda d:d.update(fault_va=0x12000000),
                       lambda d:d['integer_only'].update(decoded_bytes=0)):
            bad=deepcopy(site);mutate(bad);self.reject(output,bad)


if __name__=='__main__':unittest.main()
