#pragma once

#include "types.h"
#include "lexer.h"

typedef byte (*token_func)(word index, Token* t);

typedef struct node_ Node;

struct node_
{
	byte type;
	byte var_type;
	word type_name;
	word name;
	Node* parent;
	Node* sibling;
	Node* child;
	Node* parameters;
};

void p_init(word size, token_func f);
byte p_parse();
Node* p_root();
void p_shut();
