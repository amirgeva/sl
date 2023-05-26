HEADERS=../codegen.h ../consts.h ../dev.h ../lexer.h ../parser.h ../types.h ../datastr/strhash.h ../datastr/vector.h ../utils/memory.h
RELS=codegen.rel dev.rel lexer.rel main.rel parser.rel strhash.rel vector.rel memory.rel utils.rel
slc.bin: slc.ihx
	rm -f slc.bin
	rm -f intermediate/*
	echo intermediate > intermediate/README.md
	py ihx2bin.py slc.ihx slc.bin
	#python bin2list.py slc.bin ../z80io/os.inl
	mv *.asm intermediate
	mv *.lst intermediate
	mv *.ihx intermediate
	mv *.rel intermediate
	mv *.sym intermediate

slc.ihx: ${RELS} lowlevel.rel
	sdldz80 -m -w -i -b _CODE=0x1000 slc ${RELS} lowlevel.rel

codegen.rel: ../codegen.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../codegen.c

dev.rel: ../dev.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../dev.c

lexer.rel: ../lexer.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../lexer.c

main.rel: ../main.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../main.c

parser.rel: ../parser.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../parser.c

strhash.rel: ../datastr/strhash.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../datastr/strhash.c

vector.rel: ../datastr/vector.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../datastr/vector.c

memory.rel: ../utils/memory.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../utils/memory.c

utils.rel: ../utils/utils.c ${HEADERS}
	sdcc -mz80 -c -I.. -I../datastr -I../utils ../utils/utils.c

lowlevel.rel: ../lowlevel.asm
	sdasz80 -l -o lowlevel.rel ../lowlevel.asm

