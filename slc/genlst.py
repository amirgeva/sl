#!/usr/bin/env python3
import os.path
import shutil
import sys
from dataclasses import dataclass
from typing import Dict, NewType, List
import argh
import re
import subprocess as sp

Address = NewType('Address', int)
LineNumber = NewType('LineNumber', int)


def read_text_file(filename: str) -> List[str]:
    return open(filename).readlines()


def read_offsets(filename: str) -> Dict[Address, LineNumber]:
    res = {}
    for line in open(filename).readlines():
        values = [int(x) for x in line.strip().split()]
        res[Address(values[1])] = LineNumber(values[0])
    return res


hex_digits = '0123456789ABCDEF'


def hexstr(value, size):
    res = ['0'] * size
    idx = size - 1
    while value > 0 and idx >= 0:
        res[idx] = hex_digits[value & 0xF]
        idx -= 1
        value = value >> 4
    return ''.join(res)


@dataclass
class CodeLine:
    address: Address
    binary_offset: int
    disassembly: str
    machine_code: bytes
    source_line: LineNumber
    source_code: str


def print_machine_code(code: bytes):
    res = ' '
    for b in code:
        if res:
            res = res + ' '
        res = res + hexstr(b, 2)
    if len(res) < 20:
        res = res + ' ' * (20 - len(res))
    sys.stdout.write(res)


def generate_listing(source_code: List[str], line_offsets: Dict[Address, LineNumber],
                     disassembly: List[str], binary: bytes):
    hex_pattern = re.compile(r';([0-9a-f]+)')
    code_lines = []
    for disassembly_code_line in disassembly:
        source_code_line = ''
        binary_offset = -1
        address = Address(-1)
        source_line_number = LineNumber(-1)
        m = re.search(hex_pattern, disassembly_code_line)
        if m:
            disassembly_code_line = disassembly_code_line.split(';')[0].strip()
            address = Address(int(m.groups()[0], 16))
            binary_offset = int(address) - 0x1000
            if address in line_offsets:
                source_line_number = line_offsets.get(address)
                source_code_line = source_code[source_line_number - 1]
        else:
            disassembly_code_line = disassembly_code_line.strip()
        code_lines.append(CodeLine(address, binary_offset, disassembly_code_line, bytes(),
                                   source_line_number, source_code_line))
    n = len(code_lines)
    for i in range(n):
        cur = code_lines[i]
        if cur.binary_offset < 0:
            continue
        nxt = None
        for j in range(i + 1, n):
            if code_lines[j].binary_offset >= 0:
                nxt = code_lines[j]
                break
        next_offset = len(binary)
        if nxt is not None:
            next_offset = nxt.binary_offset
        cur.machine_code = binary[cur.binary_offset:next_offset]
    for code_line in code_lines:
        if code_line.source_code:
            print(f'{"-" * 50}')
            prefix = f'Line {code_line.source_line}'
            if len(prefix)<24:
                prefix=prefix+' '*(24-len(prefix))
            print(f"{prefix}{code_line.source_code.strip()}")
        if code_line.address >= 0:
            sys.stdout.write(hexstr(code_line.address, 4))
        else:
            sys.stdout.write('    ')
        if code_line.machine_code:
            print_machine_code(code_line.machine_code)
        else:
            sys.stdout.write(' ' * 20)
        print(code_line.disassembly)


def main(source: str):
    try:
        res=sp.run(['../buildu/slc', source], capture_output=True, check=False)
        if res.returncode!=0:
            print(res.stdout.decode('ascii'))
        else:
            code = read_text_file(source)
            line_offsets = read_offsets('line_offsets.log')
            base = os.path.splitext(source)[0]
            shutil.copyfile('out.bin',base+'.bin')
            binary = open('out.bin', 'rb').read()
            res = sp.run('z80dasm -g 4096 -a out.bin'.split(), capture_output=True, encoding='ascii')
            if res:
                disassembly = res.stdout.split('\n')
                generate_listing(code, line_offsets, disassembly, binary)
            else:
                print("Failed to disaassemble")
    except sp.CalledProcessError as e:
        print(e)

if __name__ == '__main__':
    argh.dispatch_command(main)
