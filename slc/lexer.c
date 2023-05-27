#include "lexer.h"
#include <strhash.h>
#include "vector.h"

#define INITIAL 0
#define ALPHA   1
#define NUMERIC 2
#define OPER	3

#define BUF_SIZE 16

extern byte next_byte();
static byte buffer[BUF_SIZE];
static byte bpos = 0;
static byte last_char = 0;
static byte error = 0;
static byte next_line = 0;
static word line = 0;
static word token_offset = 0;
static Vector* tokens;

word state = INITIAL;

static word str2num(const byte* b)
{
	word res = 0;
	while (*b)
	{
		if (*b >= '0' && *b <= '9')
		{
			res *= 10;
			res += (*b - '0');
		}
		else
			return 0;
		++b;
	}
	return res;
}

static int is_alpha(byte b)
{
	return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z');
}

static int is_digit(byte b)
{
	return b >= '0' && b <= '9';
}

static int is_space(byte b)
{
	return b == ' ' || b == '\t' || b == '\r';
}

void add_byte(byte b)
{
	if (bpos >= 15)
		error = 1;
	else
		buffer[bpos++] = b;
}

static void close_buffer()
{
	buffer[15] = 0;
	if (bpos < 16) buffer[bpos] = 0;
	bpos = 0;
}

int compare(const char* a, const char* b)
{
	while (1)
	{
		if (*a == 0 && *b == 0) return 0;
		if (*a == 0) return 1;
		if (*b == 0) return -1;
		if (*a == *b)
		{
			++a;
			++b;
		}
		else
			return (*a < *b ? -1 : 1);
	}
}

static byte close_alpha_token(Token* t)
{
	close_buffer();
	if (compare((const char*)buffer, "byte") == 0) { t->type = BYTE; return 1; }
	if (compare((const char*)buffer, "word") == 0) { t->type = WORD; return 1; }
	if (compare((const char*)buffer, "sbyte") == 0) { t->type = SBYTE; return 1; }
	if (compare((const char*)buffer, "sword") == 0) { t->type = SWORD; return 1; }
	if (compare((const char*)buffer, "array") == 0) { t->type = ARRAY; return 1; }
	if (compare((const char*)buffer, "addr") == 0) { t->type = ADDR; return 1; }
	if (compare((const char*)buffer, "if") == 0) { t->type = IF; return 1; }
	if (compare((const char*)buffer, "else") == 0) { t->type = ELSE; return 1; }
	if (compare((const char*)buffer, "while") == 0) { t->type = WHILE; return 1; }
	if (compare((const char*)buffer, "struct") == 0) { t->type = STRUCT; return 1; }
	if (compare((const char*)buffer, "var") == 0) { t->type = VAR; return 1; }
	if (compare((const char*)buffer, "fun") == 0) { t->type = FUN; return 1; }
	if (compare((const char*)buffer, "end") == 0) { t->type = END; return 1; }
	if (compare((const char*)buffer, "const") == 0) { t->type = CONST; return 1; }
	if (compare((const char*)buffer, "extern") == 0) { t->type = EXTERN; return 1; }
	if (compare((const char*)buffer, "return") == 0) { t->type = RETURN; return 1; }
	t->type = IDENT;
	t->value = sh_get((const char*)buffer);
	return 1;
}

static byte close_numeric_token(Token* t)
{
	close_buffer();
	t->value = str2num(buffer);
	t->type = NUMBER;
	return 1;
}

word lex_line()
{
	return line;
}

#define ADD(x) { t.type=x; t.line=line; vector_push(tokens, &t); continue; }


static void analyze()
{
	byte b;
	while (vector_size(tokens)<12)
	{
		if (next_line)
		{
			++line;
			next_line = 0;
		}
		if (error) return;
		if (last_char == 0)
			b = next_byte();
		else
		{
			b = last_char;
			last_char = 0;
		}
		if (!b)
		{
			return;
		}
		Token t;
		t.value = b;
		if (state == OPER)
		{
			state = INITIAL;
			bpos = 0;
			if (buffer[0] == '<')
			{
				if (b == '<') ADD(LSH);
				if (b == '=') ADD(LE);
				last_char = b;
				ADD(LT);
			}
			else
			if (buffer[0] == '>')
			{
				if (b == '>') ADD(RSH);
				if (b == '=') ADD(GE);
				last_char = b;
				ADD(GT);
			}
			else
			if (buffer[0] == '!')
			{
				if (b == '=')
				{
					ADD(NE);
				}
				else
				{
					error = 1;
					break;
				}
			}
		}
		if (state == INITIAL)
		{
			if (b == '#')
			{
				while (1)
				{
					b = next_byte();
					if (b == '\n') break;
					if (b == 0) return;
				}
			}
			switch (b)
			{
			case '(': ADD(LPAREN);
			case ')': ADD(RPAREN);
			case ',': ADD(COMMA);
			case '.': ADD(DOT);
			case '[': ADD(LBRACKET);
			case ']': ADD(RBRACKET);
			case '=': ADD(EQ);
			case '+': ADD(PLUS);
			case '-': ADD(MINUS);
			case '~': ADD(TILDA);
			case '&': ADD(AMP);
			case '|': ADD(PIPE);
			case '^': ADD(CARET);
			case '\n': next_line=1; ADD(EOL);
			case '<':
			case '>':
			case '!':
				add_byte(b); state = OPER; continue;
			}
			if (is_alpha(b) || b == '_')
			{
				add_byte(b);
				state = ALPHA;
			}
			else
			if (is_digit(b))
			{
				add_byte(b);
				state = NUMERIC;
			}
			else
			if (!is_space(b))
			{
				error = 1;
				return;
			}
		}
		else
		if (state == ALPHA)
		{
			if (is_alpha(b) || is_digit(b) || b == '_')
				add_byte(b);
			else
			{
				last_char = b;
				state = INITIAL;
				close_alpha_token(&t);
				ADD(t.type);
			}
		}
		else
		if (state == NUMERIC)
		{
			if (is_digit(b))
				add_byte(b);
			else
			{
				last_char = b;
				state = INITIAL;
				close_numeric_token(&t);
				ADD(t.type);
			}
		}
	}
}

void lex_init()
{
	state = INITIAL;
	last_char = 0;
	bpos = 0;
	line = 1;
	token_offset = 0;
	tokens = vector_new(sizeof(Token));
}

//word lex_size()
//{
//	return (word)vector_size(tokens);
//}

byte lex_get(word index, Token* t)
{
	if (index < token_offset) return 0; // Cannot look back more than 4 tokens
	index-=token_offset;
	if (index>=10) return 0; // Cannot look ahead too far
	if (index>=vector_size(tokens)) analyze();
	if (!vector_get(tokens, index, t)) return 0;
	if (index >= 8)
	{
		vector_erase_range(tokens, 0, 6); // allow short look back
		token_offset+=6;
	}
	return 1;
}

void lex_shut()
{
	vector_shut(tokens);
}


