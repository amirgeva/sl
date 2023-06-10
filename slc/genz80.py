from typing import List

import argh
import os


def find_c_files():
    srcs = []
    headers = []
    for cur, subdirs, files in os.walk('.'):
        if 'build' in cur:
            continue
        for name in [filename for filename in files if filename.endswith('.c')]:
            srcs.append(os.path.join(cur, name))
        for name in [filename for filename in files if filename.endswith('.h')]:
            headers.append(os.path.join(cur, name))
    return srcs, headers


def fix(paths: List[str]):
    for i in range(len(paths)):
        paths[i] = '.' + paths[i].replace('\\', '/')


def get_name(x: str):
    p = x.rfind("/")
    return x[p + 1:].split('.')[0]


def main():
    srcs, headers = find_c_files()
    fix(srcs)
    fix(headers)
    rels = [f'intermediate/{get_name(x)}.rel' for x in srcs]
    inc = '-I.. -I../datastr -I../utils'
    with open('z80/Makefile', 'w') as f:
        f.write(f'HEADERS={" ".join(headers)}\n')
        f.write(f'RELS={" ".join(rels)}')
        f.write('''
slc.bin: intermediate/slc.ihx
	rm -f slc.bin
	py ihx2bin.py intermediate/slc.ihx slc.bin

''')
        f.write('intermediate/slc.ihx: ${RELS} intermediate/lowlevel.rel\n')
        f.write('\tsdldz80 -m -w -i -b _CODE=0x1000 intermediate/slc ${RELS} intermediate/lowlevel.rel\n\n')
        for src, rel in zip(srcs, rels):
            f.write(f'{rel}: {src} ${{HEADERS}}\n')
            f.write(f'\tsdcc -mz80 -c --opt-code-size -o {rel} {inc} {src}\n\n')
        f.write('intermediate/lowlevel.rel: ../lowlevel.asm\n')
        f.write('\tsdasz80 -l -o intermediate/lowlevel.rel ../lowlevel.asm\n\n')

        f.write('clean:\n\trm intermediate/*\n\n')

if __name__ == '__main__':
    argh.dispatch_command(main)
