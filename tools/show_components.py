#!/usr/bin/env python3
"""Read-only verified map of MATRYOSHKA source files; no emulator boot needed."""
from pathlib import Path
from importlib.util import spec_from_file_location, module_from_spec
import ast
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]

COMPONENTS = (
    ('CPU M32', 'matryoshka.py', 'VirtualMachine.step', 'Python emulates instructions'),
    ('RAM + protection', 'matryoshka.py', 'VirtualMachine._guard', 'Python emulates memory'),
    ('Virtual disk', 'matryoshka.py', 'BlockDisk', 'Python emulates sector disk'),
    ('Assembler M32', 'matryoshka.py', 'assemble', 'Python assembles M32 instructions'),
    ('ROM firmware', 'firmware.asm', None, 'Guest M32 code'),
    ('Guest OS kernel', 'kernel.asm', None, 'Guest M32 code'),
    ('Guest shell', 'programs/shell.asm', None, 'Guest M32 code'),
    ('Guest Hello World', 'programs/hello.asm', None, 'Guest M32 code'),
    ('CPU M8', 'hardware_m8/m8.py', None, 'Separate incompatible ISA'),
    ('Gate ALU', 'hardware_m8/gate_alu.py', None, 'Logical model for M8'),
    ('Engine C', 'engine_lab/src/mw_engine.c', None, 'Host graphics lab only'),
    ('Engine C++', 'engine_lab/src/mw_scene.cpp', None, 'Host graphics lab only'),
    ('Engine C#', 'engine_lab/csharp/MatryoshkaDemo.cs', None, 'Host P/Invoke sample, untested .NET build'),
    ('Engine Python', 'engine_lab/python/demo.py', None, 'Host ctypes demo'),
    ('Engine Java', 'engine_lab/java/MatryoshkaEngine.java', None, 'Host JNI demo'),
)

def locations(path):
    src=(ROOT/path).read_text(encoding='utf-8')
    tree=ast.parse(src)
    locations={}
    for node in tree.body:
        if isinstance(node,(ast.FunctionDef,ast.ClassDef,ast.AsyncFunctionDef)):
            locations[node.name]=(node.lineno,node.end_lineno)
            if isinstance(node,ast.ClassDef):
                for method in node.body:
                    if isinstance(method,(ast.FunctionDef,ast.AsyncFunctionDef)):
                        locations[node.name+'.'+method.name]=(method.lineno,method.end_lineno)
    return locations

def inventory():
    out=[]
    loc=locations('matryoshka.py')
    for name,path,func,note in COMPONENTS:
        content=(ROOT/path).read_bytes()
        line=''
        if func:
            begin,end=loc[func]
            line=f':{begin}-{end}'
        out.append({'name':name,'path':path,'lines':len(content.decode('utf-8').splitlines()),'source_range':line, 'sha256':hashlib.sha256(content).hexdigest(),'status':note})
    return out

def main():
    rows=inventory()
    if '--json' in sys.argv:
        print(json.dumps(rows,ensure_ascii=False,indent=2))
    else:
        print('MATRYOSHKA SOURCE INSPECTOR — genuine project files')
        print('M32 assembly executes on the guest CPU; engine lab executes on host only.\n')
        for row in rows:
            print(f"{row['name']:<18} {row['path'] + row['source_range']:<58} ({row['lines']:>4} lines)")
        print('\nOpen standalone CODE_TOUR.html to browse the full text offline.')

if __name__=='__main__':
    main()
