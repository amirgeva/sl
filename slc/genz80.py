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
    rels = [get_name(x) + ".rel" for x in srcs]
    inc = '-I.. -I../datastr -I../utils'
    with open('z80/Makefile', 'w') as f:
        f.write(f'HEADERS={" ".join(headers)}\n')
        f.write(f'RELS={" ".join(rels)}')
        f.write('''
slc.bin: slc.ihx
	rm -f slc.bin
	rm -f intermediate/*
	echo intermediate > intermediate/README.md
	python3 ihx2bin.py slc.ihx slc.bin
	#python bin2list.py slc.bin ../z80io/os.inl
	mv *.asm intermediate
	mv *.lst intermediate
	mv *.ihx intermediate
	mv *.rel intermediate
	mv *.sym intermediate

''')
        f.write('slc.ihx: ${RELS} lowlevel.rel\n')
        f.write('\tsdldz80 -m -w -i -b _CODE=0x1000 slc ${RELS} lowlevel.rel\n\n')
        for src, rel in zip(srcs, rels):
            f.write(f'{rel}: {src} ${{HEADERS}}\n')
            f.write(f'\tsdcc -mz80 -c {inc} {src}\n\n')
        f.write('lowlevel.rel: ../lowlevel.asm\n')
        f.write('\tsdasz80 -l -o lowlevel.rel ../lowlevel.asm\n\n')


if __name__ == '__main__':
    argh.dispatch_command(main)
