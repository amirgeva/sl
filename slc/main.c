#ifdef DEV
#include <stdio.h>
#include <stdlib.h>
#endif
#include "strhash.h"
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "dev.h"
#include "optimizer.h"

char program_filename[32];

#ifdef CODE_FILE

static FILE* code_stream = 0;
static FILE* output_file = 0;
static byte  code_eof = 0;

byte next_byte()
{
	if (code_eof) return 0;
	if (!code_stream)
	{
		code_stream = fopen(program_filename, "r");
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
		code_eof = 1;
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

byte write_output(word offset, const byte* data, word length) 
{ 
	(void)offset;
	(void*)data;
	(void)length;
	return 1;
}

void close_output() {}

#endif

int main(int argc, char* argv[])
{
	if (argc > 1)
	{
		char* dst=program_filename;
		const char* src=argv[1];
		for (byte i = 0; i < 32; ++i)
		{
			*dst++ = *src;
			if (*src==0) break;
			src++;
		}
		//strcpy(program_filename, argv[1]);
	}
	else
	{
#ifdef DEV
		printf("Usage: slc <source>\n");
#endif
		return 1;
	}
	dev_init();
	alloc_init();
	sh_init();
	lex_init();
	p_init(lex_get);
	//p_parse();
	gen_init();
	//dev_print_tree(p_root());
	generate_code(p_parse, write_output);
	close_output();
/*
	opt_init(gen_get_functions(), gen_get_unknowns());
	opt_exec("out.bin");
	opt_shut();
*/
	gen_shut();
	p_shut();
	lex_shut();
	sh_shut();
	dev_shut();
	alloc_shut();
#ifdef DEV
	printf("Total memory leaked: %d\n", get_total_allocated());
	printf("Maximum allocated: %d\n", get_max_allocated());
#endif
	return 0;
}
