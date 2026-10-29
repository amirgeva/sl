HEADERS=../consts.h ../parser.h ../lexer.h ../codegen.h ../dev.h ../types.h ../datastr/strhash.h ../datastr/vector.h ../utils/memory.h ../utils/utils.h
RELS=intermediate/dev.rel intermediate/codegen.rel intermediate/lexer.rel intermediate/parser.rel intermediate/main.rel intermediate/vector.rel intermediate/strhash.rel intermediate/memory.rel intermediate/utils.rel
slc.bin: intermediate/slc.ihx
	rm -f slc.bin
	py ihx2bin.py intermediate/slc.ihx slc.bin

intermediate/slc.ihx: ${RELS} intermediate/lowlevel.rel
	sdldz80 -m -w -i -b _CODE=0x1000 intermediate/slc ${RELS} intermediate/lowlevel.rel

intermediate/dev.rel: ../dev.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/dev.rel -I.. -I../datastr -I../utils ../dev.c

intermediate/codegen.rel: ../codegen.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/codegen.rel -I.. -I../datastr -I../utils ../codegen.c

intermediate/lexer.rel: ../lexer.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/lexer.rel -I.. -I../datastr -I../utils ../lexer.c

intermediate/parser.rel: ../parser.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/parser.rel -I.. -I../datastr -I../utils ../parser.c

intermediate/main.rel: ../main.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/main.rel -I.. -I../datastr -I../utils ../main.c

intermediate/vector.rel: ../datastr/vector.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/vector.rel -I.. -I../datastr -I../utils ../datastr/vector.c

intermediate/strhash.rel: ../datastr/strhash.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/strhash.rel -I.. -I../datastr -I../utils ../datastr/strhash.c

intermediate/memory.rel: ../utils/memory.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/memory.rel -I.. -I../datastr -I../utils ../utils/memory.c

intermediate/utils.rel: ../utils/utils.c ${HEADERS}
	sdcc -mz80 -c --opt-code-size -o intermediate/utils.rel -I.. -I../datastr -I../utils ../utils/utils.c

intermediate/lowlevel.rel: ../lowlevel.asm
	sdasz80 -l -o intermediate/lowlevel.rel ../lowlevel.asm

clean:
	rm intermediate/*

