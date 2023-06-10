#pragma once

#include "types.h"
#include "lexer.h"
#include "vector.h"

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
	word		line;
	Node*		parent;
	Node*		sibling;
	Node*		child;
	Node*		parameters;
	Vector*		data;
};

void p_init(token_func f);
Node* p_parse();
Node* p_root();
void p_shut();
void release_node(Node* node);