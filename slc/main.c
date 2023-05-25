#ifdef PRINTS
#include <stdio.h>
#endif
#include "strhash.h"
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "dbg.h"

#ifdef CODE_FILE

FILE* code_stream = 0;
FILE* output_file = 0;

byte next_byte()
{
	if (!code_stream)
	{
		//code_stream = fopen("examples/tetris.sl", "r");
		code_stream = fopen("examples/sample.sl", "r");
		if (!code_stream)
		{
			fprintf(stderr, "Failed to open code file.\n");
			exit(1);
		}
	}
	int res = fgetc(code_stream);
	if (res == EOF)
	{
		fclose(code_stream);
		code_stream = 0;
		return 0;
	}
	return (byte)(res & 255);
}

byte write_output(word offset, const byte* data, word length)
{
	if (!output_file) output_file = fopen("out.bin", "wb");
	fseek(output_file, offset, SEEK_SET);
	return fwrite(data, 1, length, output_file);
}

void close_output()
{
	if (output_file)
	{
		fclose(output_file);
		output_file = 0;
	}
}

#else


/**  Sample code **/
const char* code=
"var byte a\n"
"struct Cell\n"
"  var byte width\n"
"  var byte height\n"
"end\n"
"var array 8 byte b\n"
"fun main()\n"
"  a=0\n"
"  while a<8\n"
"    a=a+1\n"
"  end\n"
"end\n";

const char* ptr=0;

byte next_byte()
{
	if (!ptr) ptr = code;
	if (!(*ptr)) return 0;
	return *ptr++;
}

byte write_output(word offset, byte* data, word length) { return 1; }

#endif

int main(/* int argc, char* argv[] */)
{
	init_alloc();
	sh_init();
	lex_init();
	p_init(lex_size(), lex_get);
	p_parse();
	gen_init();
#ifdef PRINTS
	//p_print();
	print_tree(p_root());
	//scan_sizes(p_root());
#endif
	generate_code(p_root(), write_output);
	close_output();
	gen_shut();
	p_shut();
	lex_shut();
	sh_shut();
#ifdef PRINTS
	printf("Total allocated left: %d", get_total_allocated());
#endif
	return 0;
}
