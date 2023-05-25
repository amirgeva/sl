#pragma once

#include "types.h"
#include "lexer.h"

typedef byte (*token_func)(word index, Token* t);

typedef struct base_type_
{
	byte type;		// ARRAY / VAR
	byte sub_type;	// STRUCT / PRIMITIVE
	word type_name;	// BYTE / WORD / struct_name
} BaseType;

typedef struct node_ Node;

struct node_
{
	byte		type;
	BaseType	data_type;
	word		name;
	Node*		parent;
	Node*		sibling;
	Node*		child;
	Node*		parameters;
};

void p_init(word size, token_func f);
byte p_parse();
Node* p_root();
void p_shut();
