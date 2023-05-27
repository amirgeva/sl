#pragma once

#include <types.h>
#include <consts.h>

typedef struct token
{
	byte type;
	word value;
	word line;
} Token;

void lex_init();
//word lex_size();
byte lex_get(word index, Token* t);
void lex_shut();
