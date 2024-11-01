#!/usr/bin/env python3
from typing import List
import re

import argh

line_number = 0

primitives = {'byte', 'word'}
stack = []


def process_array(tokens: List[str]) -> str:
    n = len(tokens)
    if n >= 3:
        res = f'{tokens[1]} {tokens[2]}[{tokens[0]}]'
        if n == 5 and tokens[3] == '=' and tokens[4] == '[':
            res += ' = {'
            stack.append('array')
        else:
            res += ';'
        return res
    elif n == 2:
        return f'{tokens[0]}* {tokens[1]};'
    raise SyntaxError()


def process_var(line: str) -> str:
    parts = line.split()
    if parts[1] == 'array':
        return process_array(parts[2:])
    return ' '.join(parts[1:]) + ';'


def process_const(line: str) -> str:
    parts = line.split()
    return f'constexpr word {parts[1]} = {parts[2]};'


def process_struct(line: str) -> str:
    parts = line.split()
    stack.append('struct')
    return f'{line} {{'


def process_params(params: List[str]) -> str:
    res = []
    for param in params:
        parts = param.split()
        if parts[0] == 'byte' or parts[0] == 'word':
            res.append(param)
        else:
            parts[0] += '*'
            res.append(' '.join(parts))
    return ','.join(res)


def process_function(line: str, is_extern: bool) -> str:
    m = re.fullmatch(r'fun\s+(\w+)\s*\((.*)\)', line)
    if m is None:
        raise SyntaxError()
    g: List[str] = m.groups()
    name = g[0]
    ret = 'int' if name == 'main' else 'word'
    res = f'{ret} {name}('
    params = g[1].strip()
    if params:
        groups = re.split(r'\s*,\s*', params)
        params = []
        for param in groups:
            parts = param.split()
            if parts[0] not in primitives:
                parts[0] += '&'
            params.append(f'{parts[0]} {parts[1]}')
        res += ', '.join(params)
    res += ')'
    if is_extern:
        res += ';'
    else:
        res += ' {'
        stack.append('function')
    return res


def equal_repl(m: re.Match) -> str:
    s = m.group(0)
    return s[0] + '=='


def process_conditional(line: str) -> str:
    m = re.match(r'(\w+)\s+(.+)', line)
    if m is None:
        raise SyntaxError()
    ctype: str = m.groups()[0]
    cond: str = m.groups()[1]
    cond = re.sub(f'([^<>!]=)', equal_repl, cond)
    stack.append('cond')
    return f'{ctype} ({cond}) {{'


def avoid_semi(line: str) -> bool:
    if re.match(r'(\s*\d+,?)+', line):
        return True
    return False


def process(line: str, last_line: str):
    if line == 'else':
        return '} else {'
    if line.startswith('var '):
        return process_var(line)
    if line.startswith('const '):
        return process_const(line)
    if line.startswith('struct '):
        return process_struct(line)
    if line == 'end':
        context = stack[-1]
        del stack[-1]
        if context == 'function':
            parts = last_line.split()
            if 'return' not in parts:
                return ['return 0;', '}']
        if context != 'function' and context != 'cond':
            return '};'
        else:
            return '}'
    if line == ']':
        del stack[-1]
        return '};'
    if line.startswith('extern'):
        return process_function(line[6:].strip(), True)
    if line.startswith('fun '):
        return process_function(line, False)
    if line.startswith('while '):
        return process_conditional(line)
    if line.startswith('if'):
        return process_conditional(line)
    if line and not avoid_semi(line):
        return line + ';'
    return line


def test_re():
    line = 'fun x(a,b,c,d)'
    m = re.match(r'fun (\w+)\((\w+)(,\w+)*\)', line)
    print(m.groups())


def main(input_file: str, output_file: str):
    global line_number
    with open(output_file, 'w') as f:
        f.write('#include "sim.h"\n')
        try:
            indent = 0
            last_line = ''
            for line in open(input_file).readlines():
                line_number += 1
                try:
                    comment_index = line.index('#')
                    line = line[0:comment_index].strip()
                except ValueError:
                    pass
                if line:
                    translated_lines = process(line.strip(), last_line)
                    last_line = line
                    if isinstance(translated_lines,str):
                        translated_lines=[translated_lines]
                    for translated_line in translated_lines:
                        if translated_line.startswith('}'):
                            indent -= 2
                        spaces = ''
                        if indent > 0:
                            spaces = ' ' * indent
                        f.write(f'{spaces}{translated_line}\n')
                        if translated_line.endswith('{'):
                            indent += 2

        except SyntaxError:
            print(f'Syntax error in line {line_number}')


if __name__ == '__main__':
    argh.dispatch_command(main)
