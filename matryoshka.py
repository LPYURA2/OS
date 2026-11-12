#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MATRYOSHKA v0.3.2 / T1A: guest kernel loads applications from SYSTEM.IMG.

Standard-library-only, Python 3.9+. This is an educational VM, not a security sandbox.
The kernel scheduler and syscall dispatch are assembly run by the virtual CPU.
The console, persistence and processor primitives are implemented by the Python host.
Apps are fixed-address binaries packaged into a read-only system disk; guest kernel
loads catalog and sector bytes, checks checksum, then SPAWN schedules a process.
"""
import argparse
import codecs
import os
from collections import deque
import ast
from dataclasses import dataclass, field
from pathlib import Path
import re
import shlex
import struct
import sys
import tempfile

MEMORY_SIZE = 256 * 1024
KERNEL_BASE = 0x2000
APP_BASE = 0x8000
SLOT_SIZE = 0x3000
MAX_TASKS = 8
INS_SIZE = 8
QUANTUM = 6
BLOCK_SIZE = 512
BLOCK_COUNT = 128
U32 = 0xFFFFFFFF
# Stable prototype disk catalog order; this is a temporary M32 ABI, not final .tea.
APP_ORDER = ('hello', 'counter', 'taskmgr', 'diskdemo', 'selfmod', 'shell')
CATALOG_SECTOR = 17
APP_FIRST_SECTOR = 18
APP_CATALOG_ENTRY_SIZE = 40

OP_NAMES = (
    'NOP', 'MOVI', 'MOV', 'ADD', 'ADDI', 'SUB', 'MUL', 'MODI', 'CMP',
    'JMP', 'JZ', 'JNZ', 'JLT', 'JGT', 'LOAD', 'STORE', 'TRAP',
    'IN', 'OUT', 'INIDX', 'SPAWN', 'STATUS', 'SWITCH', 'KILL',
    'WRITE', 'BLOCK', 'SETRET', 'HALT', 'RAND', 'CALL', 'RET', 'ST8', 'LD8'
)
OPS = {name: i for i, name in enumerate(OP_NAMES)}
PRIVILEGED = {'IN','OUT','INIDX','SPAWN','STATUS','SWITCH','KILL','WRITE','BLOCK','SETRET','HALT'}


def expression(text, labels):
    """Restricted arithmetic evaluator, never executes the supplied source as Python."""
    tree = ast.parse(text, mode='eval')
    def evaluate(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name) and node.id in labels:
            return labels[node.id]
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -evaluate(node.operand)
        if isinstance(node, ast.BinOp):
            left, right = evaluate(node.left), evaluate(node.right)
            if isinstance(node.op, ast.Add):
                return left + right
            if isinstance(node.op, ast.Sub):
                return left - right
            if isinstance(node.op, ast.Mult):
                return left * right
        raise ValueError('Недопустимое выражение ассемблера: ' + text)
    return evaluate(tree.body)


def string_bytes(argument):
    value = ast.literal_eval(argument)
    if not isinstance(value, str):
        raise ValueError('Строка .ascii должна быть в кавычках')
    return value.encode('utf-8')


def source_lines(source):
    lines = []
    for line_no, raw in enumerate(source.splitlines(), 1):
        stripped = raw.split(';', 1)[0].strip()
        if stripped:
            lines.append((line_no, stripped))
    return lines


def assemble(source, origin):
    """Two-pass assembler, with labels, .ascii, .zero and fixed 8-byte opcodes."""
    labels, instructions = {}, []
    pc = origin
    for line_no, text in source_lines(source):
        while ':' in text and re.match(r'^[_A-Za-z]\w*:', text):
            label, text = text.split(':', 1)
            if label in labels:
                raise ValueError(f'Строка {line_no}: повторная метка {label}')
            labels[label] = pc
            text = text.strip()
            if not text:
                break
        if not text:
            continue
        instructions.append((line_no, pc, text))
        if text.startswith('.ascii '):
            pc += len(string_bytes(text[7:].strip()))
        elif text.startswith('.zero '):
            pc += expression(text[6:].strip(), labels)
        else:
            pc += INS_SIZE
    output = bytearray()
    for line_no, address, text in instructions:
        try:
            if text.startswith('.ascii '):
                output.extend(string_bytes(text[7:].strip()))
                continue
            if text.startswith('.zero '):
                count = expression(text[6:].strip(), labels)
                if count < 0 or count > 100000:
                    raise ValueError('.zero вне допустимого диапазона')
                output.extend(bytes(count))
                continue
            operation, *rest = text.split(None, 1)
            operation = operation.upper()
            if operation not in OPS:
                raise ValueError('Неизвестная команда: ' + operation)
            arguments = [x.strip() for x in rest[0].split(',')] if rest else []
            a = b = c = imm = 0
            def reg(item):
                if not re.fullmatch(r'[Rr](?:1[0-5]|[0-9])', item):
                    raise ValueError('Ожидается регистр R0..R15: ' + item)
                return int(item[1:])
            def num(item):
                val = expression(item, labels)
                if not (-0x80000000 <= val <= U32):
                    raise ValueError('Константа должна помещаться в 32 бита')
                return val & U32
            expected = {
                'NOP': 0, 'MOVI':2, 'MOV':2, 'ADD':2, 'ADDI':2,
                'SUB':2, 'MUL':2, 'MODI':2, 'CMP':2,
                'JMP':1,'JZ':1,'JNZ':1,'JLT':1,'JGT':1,
                'LOAD':3,'STORE':3,'TRAP':1,'IN':2,'OUT':2,
                'INIDX':3,'SPAWN':1,'STATUS':2,'SWITCH':1,
                'KILL':1,'WRITE':2,'BLOCK':3,'SETRET':1,
                'HALT':0,'RAND':2,'CALL':1,'RET':0,'ST8':3,'LD8':3,
            }[operation]
            if len(arguments) != expected:
                raise ValueError(f'{operation} ожидает {expected} арг., получено {len(arguments)}')
            if operation in ('MOVI','ADDI','MODI','RAND'):
                a, imm = reg(arguments[0]), num(arguments[1])
            elif operation in ('MOV','ADD','SUB','MUL','CMP','STATUS','WRITE'):
                a, b = reg(arguments[0]), reg(arguments[1])
            elif operation in ('LOAD','STORE','ST8','LD8'):
                a, b, imm = reg(arguments[0]),reg(arguments[1]),num(arguments[2])
            elif operation in ('JMP','JZ','JNZ','JLT','JGT','TRAP','CALL'):
                imm = num(arguments[0])
            elif operation in ('IN',):
                a, imm = reg(arguments[0]), num(arguments[1])
            elif operation in ('OUT',):
                imm, a = num(arguments[0]), reg(arguments[1])
            elif operation == 'INIDX':
                a, imm, b = reg(arguments[0]),num(arguments[1]),reg(arguments[2])
            elif operation in ('SPAWN','SWITCH','KILL','SETRET'):
                a = reg(arguments[0])
            elif operation == 'BLOCK':
                a, b, c = (reg(arg) for arg in arguments)
            output.extend(struct.pack('<BBBBI', OPS[operation],a,b,c,imm))
        except (ValueError, SyntaxError, KeyError) as exc:
            raise ValueError(f'Ассемблер, строка {line_no}, адрес {address:#x}: {exc}') from exc
    if len(output) != pc - origin:
        raise RuntimeError('Ошибка размера результата двухпроходной сборки')
    return bytes(output), labels


class MachineFault(Exception):
    pass


@dataclass
class Process:
    pid: int
    name: str
    start: int
    end: int
    pc: int
    regs: list = field(default_factory=lambda: [0] * 16)
    cmp_result: int = 0
    stack: list = field(default_factory=list)
    alive: bool = True
    cycles: int = 0


class BlockDisk:
    def __init__(self, path):
        self.path = Path(path)
        if self.path.exists():
            data = self.path.read_bytes()
            if len(data) != BLOCK_SIZE * BLOCK_COUNT:
                raise MachineFault('Размер файла виртуального диска неверен')
            self.data = bytearray(data)
        else:
            self.data = bytearray(BLOCK_SIZE * BLOCK_COUNT)
            self.save()
    def transfer(self, sector, mem, ptr, write):
        if not (0 <= sector < BLOCK_COUNT):
            raise MachineFault('Диск: сектор вне диапазона')
        start = sector * BLOCK_SIZE
        if write:
            self.data[start:start+BLOCK_SIZE] = mem[ptr:ptr+BLOCK_SIZE]
            self.save()
        else:
            mem[ptr:ptr+BLOCK_SIZE] = self.data[start:start+BLOCK_SIZE]
    def save(self):
        # Crash-resistance for the host file backing the emulated block device.
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_name(self.path.name + '.tmp')
        with tmp.open('wb') as handle:
            handle.write(self.data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp, self.path)


class VirtualMachine:
    def __init__(self, folder, disk_path, quantum=QUANTUM, system_image=None):
        self.folder = Path(folder)
        self.disk = BlockDisk(disk_path)
        self.system_image_path = Path(system_image) if system_image else self.folder / 'images' / 'system.img'
        self.system_image = self.system_image_path.read_bytes()
        if len(self.system_image) != BLOCK_COUNT * BLOCK_SIZE:
            raise MachineFault('SYSTEM.IMG: неверный размер загрузочного диска')
        self.quantum = quantum
        self.reset()

    def reset(self):
        self.mem = bytearray(MEMORY_SIZE)
        self.processes = []
        self.app_images = []  # Filled by guest SPAWN, not Python boot() preloading.
        self.requested_app_indexes = []
        self.booted = False
        self.mode = 'kernel'
        self.pc = 0
        self.regs = [0] * 16
        self.kernel_regs = self.regs
        self.kernel_cmp = 0
        self.cmp_result = 0
        self.stack = []
        self.kernel_stack = self.stack
        self.current_pid = None
        self.last_pid = -1
        self.last_reason = 0
        self.last_regs = [0]*16
        self.kernel_resume = KERNEL_BASE
        self.user_quantum = 0
        self.cycles = 0
        self.timers = 0
        self.syscalls = 0
        self.console = []
        self.halted = False
        self.trace = []
        self.keyboard = deque()
        self.keyboard_idle = False
        self.boot_sector = 0
        self.boot_dest = 0
        self.boot_status = 0
        self.boot_reads = []
        self.terminal_decoder = codecs.getincrementaldecoder('utf-8')('replace')
        self.screen_width = 40
        self.screen_height = 14
        self.video = [list(" " * self.screen_width) for _ in range(self.screen_height)]
        self.video_x = 0
        self.video_y = 0

    def boot(self, apps=None):
        self.reset()
        # Hardware host installs ROM only. Guest ROM and guest kernel read
        # SYSTEM.IMG; application slots are completely empty at time zero.
        firmware = (self.folder / 'images' / 'firmware.bin').read_bytes()
        if len(firmware) > KERNEL_BASE:
            raise MachineFault('Firmware не помещается в ROM')
        self.mem[:len(firmware)] = firmware
        app_names = list(APP_ORDER[:-1]) if apps is None else list(apps)
        if len(app_names) > MAX_TASKS:
            raise MachineFault('Слишком много программ')
        if len(set(app_names)) != len(app_names):
            raise MachineFault('Повторяющееся имя программы')
        for name in app_names:
            if name not in APP_ORDER:
                raise MachineFault('Неизвестный идентификатор приложения: ' + name)
            self.requested_app_indexes.append(APP_ORDER.index(name))
        self.booted = True
        self.trace.append('host: ROM only, RAM application slots empty; guest reads app catalog + sectors')

    def _guard(self, address, size, as_user=None):
        if address < 0 or size < 0 or address + size > MEMORY_SIZE:
            raise MachineFault(f'Недопустимый адрес памяти {address:#x} ({size} байт)')
        user = self.mode == 'user' if as_user is None else as_user
        if user:
            if self.current_pid is None:
                raise MachineFault('Отсутствует пользовательский процесс')
            proc = self.processes[self.current_pid]
            if address < proc.start or address + size > proc.end:
                raise MachineFault(f'Защита памяти: PID {proc.pid}, адрес {address:#x}')

    def _trap(self, reason):
        assert self.mode == 'user'
        proc = self.processes[self.current_pid]
        proc.pc = self.pc
        proc.regs = self.regs.copy()
        proc.cmp_result = self.cmp_result
        proc.stack = self.stack.copy()
        self.last_pid = proc.pid
        self.last_reason = reason
        self.last_regs = self.regs.copy()
        self.current_pid = None
        self.mode = 'kernel'
        self.regs = self.kernel_regs
        self.cmp_result = self.kernel_cmp
        self.stack = self.kernel_stack
        self.pc = self.kernel_resume
        self.user_quantum = 0
        if reason != 0:
            self.syscalls += 1
        else:
            self.timers += 1

    def _port(self, port, index=0):
        if port == 0: return self.last_reason
        if port == 2: return self.last_regs[0]
        if port == 3: return self.last_regs[1]
        if port == 4: return self.last_pid & U32
        if port == 5: return self.last_regs[2]
        if port == 10: return len(self.requested_app_indexes)
        if port == 11:
            return self.requested_app_indexes[index] if 0 <= index < len(self.requested_app_indexes) else U32
        if port == 12: return sum(proc.alive for proc in self.processes)
        if port == 13: return len(self.processes)
        if port == 20: return self.cycles
        if port == 53: return self.boot_status
        if port == 30:
            if self.keyboard:
                self.keyboard_idle = False
                return self.keyboard.popleft()
            self.keyboard_idle = True
            return 0
        raise MachineFault(f'Нет такого порта устройства: {port}')

    def feed_keys(self, text):
        self.keyboard_idle = False
        for octet in text.encode('utf-8'):
            self.keyboard.append(octet)

    def render_video(self):
        return '\n'.join(''.join(row) for row in self.video)

    def _emit(self, text):
        self.console.append(text)
        for char in text:
            if char == '\n':
                self.video_x = 0
                self.video_y += 1
            elif char == '\r':
                self.video_x = 0
            else:
                if self.video_x >= self.screen_width:
                    self.video_x = 0
                    self.video_y += 1
                if self.video_y >= self.screen_height:
                    self.video.pop(0)
                    self.video.append(list(' '*self.screen_width))
                    self.video_y = self.screen_height-1
                self.video[self.video_y][self.video_x] = char if char.isprintable() else '?'
                self.video_x += 1
            if self.video_y >= self.screen_height:
                self.video.pop(0)
                self.video.append(list(' '*self.screen_width))
                self.video_y = self.screen_height-1

    def clear_video(self):
        self.video = [list(' '*self.screen_width) for _ in range(self.screen_height)]
        self.video_x = 0
        self.video_y = 0

    def step(self):
        if self.halted:
            return
        if self.cycles >= 2_000_000:
            raise MachineFault('Аварийный лимит: 2 млн инструкций')
        self._guard(self.pc, INS_SIZE)
        if self.pc % INS_SIZE:
            raise MachineFault(f'PC не выровнен по {INS_SIZE} байтам: {self.pc:#x}')
        opcode, a,b,c,imm = struct.unpack_from('<BBBBI', self.mem, self.pc)
        if opcode >= len(OP_NAMES) or a>15 or b>15 or c>15:
            raise MachineFault(f'Испорченная инструкция по адресу {self.pc:#x}')
        op = OP_NAMES[opcode]
        if self.mode == 'user' and op in PRIVILEGED:
            raise MachineFault(f'PID {self.current_pid}: запрещённая инструкция {op}')
        self.pc += INS_SIZE
        self.cycles += 1
        r = self.regs
        signed = lambda x: x if x < 0x80000000 else x-0x100000000
        if op == 'NOP': pass
        elif op == 'MOVI': r[a] = imm
        elif op == 'MOV': r[a] = r[b]
        elif op == 'ADD': r[a] = (r[a]+r[b]) & U32
        elif op == 'ADDI': r[a] = (r[a]+imm) & U32
        elif op == 'SUB': r[a] = (r[a]-r[b]) & U32
        elif op == 'MUL': r[a] = (r[a]*r[b]) & U32
        elif op == 'MODI':
            if imm == 0: raise MachineFault('Деление по модулю нуля')
            r[a] %= imm
        elif op == 'CMP': self.cmp_result = (signed(r[a]) > signed(r[b])) - (signed(r[a]) < signed(r[b]))
        elif op == 'JMP': self.pc = imm
        elif op == 'JZ':
            if self.cmp_result == 0: self.pc = imm
        elif op == 'JNZ':
            if self.cmp_result != 0: self.pc = imm
        elif op == 'JLT':
            if self.cmp_result < 0: self.pc = imm
        elif op == 'JGT':
            if self.cmp_result > 0: self.pc = imm
        elif op == 'LOAD':
            address = (r[b]+imm) & U32
            self._guard(address,4)
            r[a] = struct.unpack_from('<I',self.mem,address)[0]
        elif op == 'STORE':
            address = (r[b]+imm) & U32
            self._guard(address,4)
            if address < KERNEL_BASE: raise MachineFault('ROM нельзя изменять')
            struct.pack_into('<I',self.mem,address,r[a])
        elif op == 'ST8':
            address = (r[b]+imm) & U32
            self._guard(address,1)
            if address < KERNEL_BASE: raise MachineFault('ROM нельзя изменять')
            self.mem[address] = r[a] & 0xFF
        elif op == 'LD8':
            address = (r[b]+imm) & U32
            self._guard(address,1)
            r[a] = self.mem[address]
        elif op == 'TRAP':
            if self.mode != 'user': raise MachineFault('TRAP разрешён только из user mode')
            self._trap(imm)
            return
        elif op == 'IN': r[a] = self._port(imm)
        elif op == 'OUT':
            if imm == 1:
                emitted = self.terminal_decoder.decode(bytes([r[a] & 0xFF]))
                if emitted:
                    self._emit(emitted)
            elif imm == 40: self.clear_video()
            elif imm == 50: self.boot_sector = r[a]
            elif imm == 51: self.boot_dest = r[a]
            elif imm == 52:
                self.boot_status = 0
                if r[a] == 1 and 0 <= self.boot_sector < BLOCK_COUNT and (
                    KERNEL_BASE <= self.boot_dest and self.boot_dest + BLOCK_SIZE <= APP_BASE + SLOT_SIZE * MAX_TASKS
                ):
                    start = self.boot_sector * BLOCK_SIZE
                    self.mem[self.boot_dest:self.boot_dest+BLOCK_SIZE] = self.system_image[start:start+BLOCK_SIZE]
                    self.boot_reads.append((self.boot_sector,self.boot_dest))
                    self.boot_status = 1
            else: raise MachineFault('Неизвестный порт OUT')
        elif op == 'INIDX': r[a] = self._port(imm, r[b])
        elif op == 'SPAWN':
            if len(self.processes) >= MAX_TASKS:
                raise MachineFault('Таблица процессов заполнена')
            address = r[a]
            if address < APP_BASE or (address - APP_BASE) % SLOT_SIZE:
                raise MachineFault('SPAWN: неверный адрес входа в слот')
            app_index = (address - APP_BASE) // SLOT_SIZE
            if app_index >= len(APP_ORDER):
                raise MachineFault('SPAWN: несуществующий слот')
            if len(self.processes) >= MAX_TASKS or any(proc.alive and proc.start == address for proc in self.processes):
                r[a] = U32  # Guest-visible failure, not host crash.
                return
            name = APP_ORDER[app_index]
            pid = len(self.processes)
            self.processes.append(Process(pid,name,address,address+SLOT_SIZE,address))
            self.app_images.append((name,address,SLOT_SIZE))
            self.trace.append(f'guest kernel: SPAWN pid={pid} name={name} after guest disk load')
            r[a] = pid  # Guest kernel owns slot -> PID mapping.
        elif op == 'STATUS':
            pid = r[b]
            r[a] = int(0 <= pid < len(self.processes) and self.processes[pid].alive)
        elif op == 'SWITCH':
            pid = r[a]
            if not (0 <= pid < len(self.processes) and self.processes[pid].alive):
                raise MachineFault('Ядро переключилось на отсутствующий процесс')
            self.kernel_resume = self.pc
            self.kernel_regs = r
            self.kernel_cmp = self.cmp_result
            self.kernel_stack = self.stack
            proc = self.processes[pid]
            self.regs = proc.regs.copy()
            self.cmp_result = proc.cmp_result
            self.stack = proc.stack.copy()
            self.pc = proc.pc
            self.mode = 'user'
            self.current_pid = pid
            self.user_quantum = 0
            self.trace.append(f'guest kernel: SWITCH -> pid={pid}')
        elif op == 'KILL':
            pid = r[a]
            if not (0 <= pid < len(self.processes)):
                raise MachineFault('Ядро попыталось завершить неверный PID')
            self.processes[pid].alive = False
            self.trace.append(f'guest kernel: KILL pid={pid}')
        elif op == 'WRITE':
            address, length = r[a], r[b]
            if length > 4096: raise MachineFault('Длина WRITE превышает 4096 байт')
            self._guard(address,length,as_user=False)
            if self.last_reason == 5 and 0 <= self.last_pid < len(self.processes):
                owner = self.processes[self.last_pid]
                if not (owner.start <= address and address+length <= owner.end):
                    raise MachineFault('SYS_WRITE: буфер не принадлежит процессу-заявителю')
            self._emit(self.mem[address:address+length].decode('utf-8','replace'))
        elif op == 'BLOCK':
            sector, address, write = r[a],r[b],r[c]
            if write not in (0,1): raise MachineFault('BLOCK: режим должен быть 0 или 1')
            if not (0 <= self.last_pid < len(self.processes)):
                raise MachineFault('BLOCK: нет процесса-заявителя')
            target = self.processes[self.last_pid]
            if not (target.start <= address and address+BLOCK_SIZE <= target.end):
                raise MachineFault('BLOCK: буфер должен принадлежать процессу-заявителю')
            self.disk.transfer(sector,self.mem,address,write)
            self.trace.append(f'guest kernel: BLOCK sector={sector} write={write}')
        elif op == 'SETRET':
            if not (0 <= self.last_pid < len(self.processes)):
                raise MachineFault('SETRET: нет процесса-заявителя')
            self.processes[self.last_pid].regs[0] = r[a]
        elif op == 'HALT': self.halted = True
        elif op == 'RAND':
            # Deterministic pseudo-random hardware counter for reproducible demos.
            r[a] = (self.cycles * 1103515245 + 12345) % (imm or 1)
        elif op == 'CALL':
            if len(self.stack) >= 64: raise MachineFault('Переполнение стека вызовов')
            self.stack.append(self.pc)
            self.pc = imm
        elif op == 'RET':
            if not self.stack: raise MachineFault('Пустой стек возврата')
            self.pc = self.stack.pop()
        if self.mode == 'user':
            self.processes[self.current_pid].cycles += 1
            self.user_quantum += 1
            if self.user_quantum >= self.quantum:
                self._trap(0)

    def run(self, max_steps=100000):
        count = 0
        while not self.halted and count < max_steps:
            self.step()
            count += 1
        return count

    def run_until_keyboard_wait(self, max_steps=60000):
        """Run a guest until it polls an empty keyboard, halts, or hits the budget."""
        count = 0
        while not self.halted and not (self.keyboard_idle and not self.keyboard) and count < max_steps:
            self.step()
            count += 1
        return count

    def display_ps(self):
        lines = ['PID  СТАТУС    КОМАНДА     PC       ТИКИ']
        for p in self.processes:
            state = 'RUN' if p.alive else 'EXIT'
            lines.append(f'{p.pid:<4} {state:<9} {p.name:<11} {p.pc:#08x} {p.cycles:>6}')
        return '\n'.join(lines)

    def disasm(self, address, count=8):
        lines = []
        for i in range(max(0,min(count,32))):
            pos = address+i*INS_SIZE
            if pos<0 or pos+INS_SIZE>MEMORY_SIZE: break
            o,a,b,c,n = struct.unpack_from('<BBBBI',self.mem,pos)
            name = OP_NAMES[o] if o < len(OP_NAMES) else f'BAD({o})'
            lines.append(f'{pos:06x}: {name:<8} a={a:<2} b={b:<2} c={c:<2} imm=0x{n:08x}')
        return '\n'.join(lines)


def selftest(project):
    with tempfile.TemporaryDirectory(prefix='matryoshka-test-') as temporary:
        vm = VirtualMachine(project,Path(temporary)/'disk.img')
        vm.boot()
        assert not any(vm.mem[APP_BASE:APP_BASE + SLOT_SIZE * len(APP_ORDER)]), 'Host preloaded app RAM'
        ticks = vm.run()
        output = ''.join(vm.console)
        for signature in ('POST OK - loading kernel from SYSTEM.IMG','MATRYOSHKA/OS','Hello from USER MODE','counter: 1','counter: 5','[ps] PID ','DISK: persistent data','executed patched instructions: ABCDEFGH'):
            assert signature in output, (signature,output)
        assert vm.boot_reads[0] == (0,0x7000), vm.boot_reads
        assert vm.boot_reads[1] == (1,KERNEL_BASE), vm.boot_reads
        assert (CATALOG_SECTOR,0x7000) in vm.boot_reads, vm.boot_reads
        assert any(sector >= APP_FIRST_SECTOR and dest >= APP_BASE for sector,dest in vm.boot_reads)
        assert len(vm.app_images) == 5
        assert b'[FIRMWARE] Kernel checksum OK -' in output.encode('utf-8')
        assert vm.halted and len(vm.processes)==5
        assert all(not p.alive for p in vm.processes)
        assert vm.timers > 0, 'Аппаратные таймерные прерывания не сработали'
        assert vm.syscalls > 0
        assert bytes(vm.disk.data[3*BLOCK_SIZE:3*BLOCK_SIZE+21]).startswith(b'DISK: persistent data')
        assert any('SPAWN' in line for line in vm.trace)
        assert any('SWITCH' in line for line in vm.trace)
        assert any('BLOCK' in line for line in vm.trace)
        # Validate memory protection even when malicious code comes FROM SYSTEM.IMG.
        evil_src = 'MOVI R0, 8192\nLOAD R1, R0, 0\nTRAP 3\n'
        evil, _ = assemble(evil_src,APP_BASE)
        evil_disk = bytearray((project/'images'/'system.img').read_bytes())
        catalog_at = CATALOG_SECTOR * BLOCK_SIZE
        hello_entry = catalog_at + 16
        hello_sector = struct.unpack_from('<I',evil_disk,hello_entry+20)[0]
        sectors = struct.unpack_from('<I',evil_disk,hello_entry+24)[0]
        assert sectors == 1
        segment = evil.ljust(BLOCK_SIZE,b'\x00')
        evil_disk[hello_sector*BLOCK_SIZE:(hello_sector+1)*BLOCK_SIZE] = segment
        evil_sum = sum(row[0] for row in struct.iter_unpack('<I',segment)) & U32
        struct.pack_into('<I',evil_disk,hello_entry+32,evil_sum)
        evil_path = Path(temporary)/'evil_app.img'
        evil_path.write_bytes(evil_disk)
        vm2 = VirtualMachine(project,Path(temporary)/'disk2.img',system_image=evil_path)
        vm2.boot(['hello'])
        try:
            vm2.run()
        except MachineFault as error:
            assert 'Защита памяти' in str(error), str(error)
        else:
            raise AssertionError('Защита памяти не остановила программу, загруженную с диска')
        print('SELFTEST: PASS')
        print('OS kernel: assembled guest machine code')
        print('Processes:',len(vm.processes),'Instructions:',ticks,'Timer preemptions:',vm.timers,'Syscalls:',vm.syscalls)
        # The guest command parser must execute INSIDE CPU user mode.
        shell_vm = VirtualMachine(project,Path(temporary)/'shell_disk.img')
        shell_vm.boot(['shell'])
        shell_vm.feed_keys('help\nps\ninfo\nclear\nexit\n')
        shell_vm.run(150000)
        shell_out = ''.join(shell_vm.console)
        assert 'DISK LOADED GUEST SHELL' in shell_out, shell_out
        assert 'ls load save <text> rm' in shell_out, shell_out
        assert 'PID   STATE' in shell_out, shell_out
        assert '256 KiB RAM' in shell_out, shell_out
        assert shell_vm.halted, shell_out
        assert len(shell_vm.video)==shell_vm.screen_height
        # User initiates a new process via a syscall. Code must come from disk.
        exec_vm = VirtualMachine(project,Path(temporary)/'exec_data.img')
        exec_vm.boot(['shell'])
        assert not any(exec_vm.mem[APP_BASE:APP_BASE+SLOT_SIZE*len(APP_ORDER)])
        exec_vm.feed_keys('run 0\nrun 9\nps\nexit\n')
        exec_vm.run(150000)
        exec_out = ''.join(exec_vm.console)
        assert 'Application launched from system disk.' in exec_out, exec_out
        assert 'Hello from USER MODE' in exec_out, exec_out
        assert 'Unknown app ID' in exec_out, exec_out
        assert any(row[0] == APP_FIRST_SECTOR and row[1] == APP_BASE for row in exec_vm.boot_reads)
        assert len(exec_vm.processes) == 2
        assert exec_vm.halted
        # A real guest-shell disk write, CPU reboot, read, remove, and second read.
        note_disk = Path(temporary)/'note_persist.img'
        note1 = VirtualMachine(project,note_disk)
        note1.boot(['shell'])
        note1.feed_keys('save Привет, Матрёшка!\nls\nload\nexit\n')
        note1.run(150000)
        note_out1 = ''.join(note1.console)
        assert 'Saved NOTE.TXT on virtual disk.' in note_out1, note_out1
        assert 'NOTE.TXT\n' in note_out1, note_out1
        assert 'Привет, Матрёшка!' in note_out1, note_out1
        note2 = VirtualMachine(project,note_disk)
        note2.boot(['shell'])
        note2.feed_keys('load\nrm\nls\nexit\n')
        note2.run(150000)
        note_out2 = ''.join(note2.console)
        assert 'Привет, Матрёшка!' in note_out2, note_out2
        assert 'NOTE.TXT removed.' in note_out2, note_out2
        assert 'No NOTE.TXT' in note_out2, note_out2
        note3 = VirtualMachine(project,note_disk)
        note3.boot(['shell'])
        note3.feed_keys('load\nexit\n')
        note3.run(150000)
        assert 'No NOTE.TXT' in ''.join(note3.console)
        # Negative boot tests: corrupt header and corrupt kernel payload.
        valid_system = (project/'images'/'system.img').read_bytes()
        for name, pos in [('header',0),('kernel',BLOCK_SIZE + 12)]:
            bad_image = bytearray(valid_system)
            bad_image[pos] ^= 0x01
            corrupt_path = Path(temporary) / (name + '_corrupt.img')
            corrupt_path.write_bytes(bad_image)
            bad_vm = VirtualMachine(project,Path(temporary)/(name+'_data.img'),system_image=corrupt_path)
            bad_vm.boot(['shell'])
            bad_vm.run(60000)
            assert bad_vm.halted and not bad_vm.processes
            assert 'BOOT ERROR' in ''.join(bad_vm.console), name
        # Negative application-loader tests: independent damage must not launch tasks.
        for name, patch_off in [('catalog_magic', CATALOG_SECTOR*BLOCK_SIZE),
                                ('app_checksum', hello_sector*BLOCK_SIZE+12),
                                ('catalog_address', catalog_at+16+16),
                                ('catalog_sector', catalog_at+16+20)]:
            corrupted = bytearray(valid_system)
            corrupted[patch_off] ^= 1
            path_bad = Path(temporary)/(name+'_app.img')
            path_bad.write_bytes(corrupted)
            bad_app_vm = VirtualMachine(project,Path(temporary)/(name+'_user.img'),system_image=path_bad)
            bad_app_vm.boot(['hello'])
            bad_app_vm.run(60000)
            assert bad_app_vm.halted, name
            assert not bad_app_vm.processes, name
            assert 'APP LOAD ERROR' in ''.join(bad_app_vm.console), name
        print('ROM -> SYSTEM.IMG -> kernel RAM; header/checksum rejection: PASS')
        print('KERNEL -> disk app catalog -> app RAM -> checksum -> SPAWN: PASS')
        print('App corruption/catalog validation; user memory isolation: PASS')
        print('Guest NOTE.TXT sector-80 persistence / delete / reboot: PASS')
        print('Guest keyboard/shell/display: PASS')
        print('Disk: persistent 128 x 512 byte sectors; memory protection: PASS')
        print('Scheduler and syscall routing: guest kernel; devices: Python host')


def monitor(vm):
    print('MATRYOSHKA debug console. help — команды. boot — загрузить гостевую ОС.')
    while True:
        try:
            raw = input('matryoshka> ').strip()
        except (EOFError,KeyboardInterrupt):
            print('\nВыход.')
            return
        if not raw:
            continue
        parts = raw.split()
        command = parts[0].lower()
        try:
            if command in ('q','quit','exit'):
                return
            elif command == 'help':
                print('boot [имена приложений] | run [шаги] | step [шаги] | ps | screen | trace [n] | regs | mem АДРЕС [n] | dis АДРЕС [n] | disk СЕКТОР | apps | quit')
            elif command == 'boot':
                vm.boot(parts[1:] if len(parts)>1 else None)
                print(f'Firmware ROM loaded; guest reads kernel and selected apps from SYSTEM.IMG; запрошено приложений: {len(vm.requested_app_indexes)}')
            elif command in ('run','step'):
                if not vm.booted:
                    print('Сначала выполни boot')
                    continue
                limit = int(parts[1],0) if len(parts)>1 else (100000 if command=='run' else 1)
                if limit <=0 or limit>500000: raise ValueError('Число шагов: от 1 до 500000')
                previous = len(vm.console)
                count = vm.run(limit)
                print(''.join(vm.console[previous:]),end='')
                print(f'\n[CPU] +{count} instr; total={vm.cycles}; mode={vm.mode}; timers={vm.timers}; halted={vm.halted}')
            elif command == 'ps': print(vm.display_ps())
            elif command == 'screen': print(vm.render_video())
            elif command == 'trace':
                n = int(parts[1],0) if len(parts)>1 else 18
                print('\n'.join(vm.trace[-min(n,200):]))
            elif command == 'regs':
                print(f'MODE={vm.mode} PC={vm.pc:#x} CURRENT_PID={vm.current_pid} CMP={vm.cmp_result}')
                print('  '.join(f'R{i}={v:#x}' for i,v in enumerate(vm.regs)))
            elif command == 'mem':
                address = int(parts[1],0)
                length = int(parts[2],0) if len(parts)>2 else 64
                if address<0 or length<0 or length>256 or address+length>MEMORY_SIZE: raise ValueError('Некорректный диапазон')
                print(vm.mem[address:address+length].hex(' ',1))
            elif command == 'dis': print(vm.disasm(int(parts[1],0),int(parts[2],0) if len(parts)>2 else 8))
            elif command == 'disk':
                sector = int(parts[1],0)
                if not 0<=sector<BLOCK_COUNT: raise ValueError('Недопустимый сектор')
                raw = vm.disk.data[sector*BLOCK_SIZE:(sector+1)*BLOCK_SIZE]
                print(f'SECTOR {sector}:', raw[:96].hex(' '))
                print('ASCII:', repr(raw[:96].decode('ascii','replace')))
            elif command == 'apps':
                print('\n'.join(f'{APP_ORDER[i]:<12} slot={i} @ {APP_BASE+i*SLOT_SIZE:#x}' for i in vm.requested_app_indexes) or 'Нет выбранных приложений')
            else:
                print('Неизвестная команда; help для списка')
        except (MachineFault,ValueError,IndexError) as err:
            print('ERROR:',err)


def main():
    parser = argparse.ArgumentParser(description='MATRYOSHKA: компьютер и гостевая ОС внутри Python')
    parser.add_argument('--selftest', action='store_true',help='Самопроверка')
    parser.add_argument('--run',action='store_true',help='Без меню запустить ОС и все программы')
    parser.add_argument('--os',action='store_true',help='Запуск гостевой командной оболочки')
    parser.add_argument('--disk',default=None,help='Путь к файлу пользовательского диска')
    parser.add_argument('--system-image',default=None,help='Путь к отдельному загрузочному SYSTEM.IMG (только чтение)')
    parser.add_argument('--apps',default=None,help='Имена программ через запятую, например hello,counter')
    parser.add_argument('--quantum',default=QUANTUM,type=int,help='Квант таймера в инструкциях (1..1000)')
    args = parser.parse_args()
    folder = Path(__file__).resolve().parent
    if args.selftest:
        selftest(folder)
        return
    if not 1<=args.quantum<=1000:
        parser.error('quantum: 1..1000')
    path = Path(args.disk) if args.disk else folder/'matryoshka_user_disk.img'
    vm = VirtualMachine(folder,path,args.quantum,args.system_image)
    if args.os:
        vm.boot(['shell'])
        vm.run_until_keyboard_wait()
        print(''.join(vm.console), end='')
        while not vm.halted:
            try:
                line = input()
            except (EOFError, KeyboardInterrupt):
                line = 'exit'
            vm.feed_keys(line+'\n')
            previous = len(vm.console)
            vm.run_until_keyboard_wait()
            print(''.join(vm.console[previous:]),end='',flush=True)
    elif args.run:
        vm.boot(args.apps.split(',') if args.apps else None)
        steps = vm.run()
        print(''.join(vm.console),end='')
        print(f'\n[HALT={vm.halted} | PROCESSES={len(vm.processes)} | INSTRUCTIONS={steps} | TIMER_IRQ={vm.timers}]')
    else:
        monitor(vm)


if __name__ == '__main__':
    try:
        main()
    except (MachineFault,ValueError,OSError) as error:
        print('MATRYOSHKA ERROR:',error,file=sys.stderr)
        sys.exit(1)
