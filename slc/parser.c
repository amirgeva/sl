#include "parser.h"
#include "strhash.h"
#include "memory.h"
#include "vector.h"

#define CONTEXT_LIMIT 16


#define ERROR_RET { error=1; if (node) release_node(node); return 0; }
#define VERIFY(x,y) if (x!=y) ERROR_RET;
#define NEXT_TOKEN if (!get_token(cur_index++, &t)) ERROR_RET
#define EXPECT(x) { NEXT_TOKEN; if (t.type != x) ERROR_RET; }

typedef struct constant_
{
	word name;
	word value;
} Constant;

Vector* constants;
Node root;
Node* cur_node=0;

typedef Node* (*state)();

typedef struct context_
{
	state ctx_state;
	Node* node;
} Context;


static Context context_stack[CONTEXT_LIMIT];
static byte  error = 0;
static byte  context_depth = 0xFF;
static word  cur_index = 0;
static word  tokens_size;
static token_func get_lex_token = 0;
static word  line_number = 1;

byte get_token(word index, Token* t)
{
	if (index >= tokens_size) return 0;
	if (!get_lex_token(index, t)) return 0;
	if (t->type == IDENT)
	{
		Constant c;
		for (word i = 0; i < vector_size(constants); ++i)
		{
			vector_get(constants, i, &c);
			if (c.name == t->value)
			{
				t->type = NUMBER;
				t->value = c.value;
				break;
			}
		}
	}
	if (t->type == EOL) ++line_number;
	return 1;
}

//#define PULL(x) if (!pull(x)) ERROR_RET

void push_context(state c, Node* node)
{
	if (context_depth == 0xFF || context_depth < (CONTEXT_LIMIT - 1))
	{
		++context_depth;
		context_stack[context_depth].ctx_state = c;
		context_stack[context_depth].node = cur_node;
		cur_node = node;
	}
	else
		error = 1;
}

void pop_context()
{
	if (context_depth > 0)
	{
		cur_node = context_stack[context_depth].node;
		--context_depth;
	}
	else
		error = 1;
}
 
state context()
{
	return context_stack[context_depth].ctx_state;
}

Node* parse_var();


void add_child(Node* parent, Node* child)
{
	Node* last_child = parent->child;
	if (!last_child) parent->child = child;
	else
	{
		Node* next_child = last_child->sibling;
		while (next_child)
		{
			last_child = next_child;
			next_child = last_child->sibling;
		}
		last_child->sibling = child;
	}
}

void add_parameter(Node* node, Node* param)
{
	Node* last_param = node->parameters;
	if (!last_param) node->parameters = param;
	else
	{
		Node* next_param = last_param->sibling;
		while (next_param)
		{
			last_param = next_param;
			next_param = last_param->sibling;
		}
		last_param->sibling = param;
	}
}

void init_node(Node* node, Node* parent)
{
	node->parent = parent;
	node->sibling = 0;
	node->child = 0;
	node->parameters = 0;
	node->type = 0;
	node->var_type = 0;
	node->type_name = 0;
	node->name = 0;
	if (parent)
		add_child(parent, node);
}

Node* allocate_node(byte type, Node* parent)
{
	Node* new_node=(Node*)allocate(sizeof(Node));
	init_node(new_node, parent);
	new_node->type = type;
	return new_node;
}

void release_node(Node* node)
{
	if (!node) return;
	if (node->child) release_node(node->child);
	if (node->sibling) release_node(node->sibling);
	if (node->parameters) release_node(node->parameters);
	release(node);
}

void release_root()
{
	if (root.child) release_node(root.child);
	root.child = 0;
}

Node* parse_lvalue();
Node* parse_expression();
Node* parse_call();

Node* parse_value()
{
	Token t;
	Node* node = 0;
	NEXT_TOKEN
	if (t.type == NUMBER)
	{
		node = allocate_node(NUMBER, 0);
		node->name = t.value;
		return node;
	}
	--cur_index;
	return parse_lvalue();
}

Node* parse_lvalue()
{
	Token t;
	Node* node = allocate_node(IDENT, 0);
	NEXT_TOKEN
	node->name = t.value;
	NEXT_TOKEN
	if (t.type == LPAREN)
	{
		cur_index -= 2; // Undo ident and LPAREN
		release_node(node);
		node = parse_call();
		if (!node) ERROR_RET;
		return node;
	}
	while (1)
	{
		if (t.type == EQ)
		{
			--cur_index;
			return node;
		}
		if (t.type == LBRACKET)
		{
			Node* bracket_node = allocate_node(INDEX, 0);
			add_child(bracket_node, node);
			node = bracket_node;
			Node* index_node = parse_expression();
			if (!index_node) ERROR_RET;
			add_child(node, index_node);
			EXPECT(RBRACKET);
		}
		else
		if (t.type == DOT)
		{
			Node* dot_node = allocate_node(DOT, 0);
			add_child(dot_node, node);
			node = dot_node;
			EXPECT(IDENT);
			Node* member = allocate_node(IDENT, 0);
			member->name = t.value;
			add_child(node, member);
		}
		else
		{
			--cur_index;
			break;
		}
		NEXT_TOKEN
	}
	return node;
}

Node* parse_expression()
{
	Token t;
	Node* node = parse_value();
	if (!node) return 0;
	NEXT_TOKEN
	if (t.type == PLUS || t.type == MINUS || t.type == LSH || t.type == RSH ||
		t.type == GT || t.type == LT || t.type == GE || t.type == LE ||
		t.type == EQ || t.type == AMP || t.type == PIPE || t.type==CARET)
	{
		Node* oper = allocate_node(t.type, 0);
		add_child(oper, node);
		node = oper;
		Node* right = parse_expression();
		if (right)
			add_child(node, right);
		else ERROR_RET;
	}
	else --cur_index;
	return node;
}

Node* parse_call()
{
	Token t;
	Node* node = allocate_node(CALL, 0);
	NEXT_TOKEN
	node->name = t.value;
	EXPECT(LPAREN);
	byte param_count = 0;
	while (1)
	{
		NEXT_TOKEN
		if (t.type == RPAREN) return node;
		if (param_count > 0)
		{
			if (t.type != COMMA) ERROR_RET;
		}
		else --cur_index;
		Node* param = parse_value();
		if (!param) ERROR_RET;
		add_parameter(node, param);
		++param_count;
	}
}

Node* parse_statement()
{
	int new_block = 0;
	Node* node = 0;
	Token t;
	NEXT_TOKEN
	if (t.type == EOL)
		return cur_node;
	if (t.type == RETURN)
	{
		node = allocate_node(RETURN, 0);
		Node* expr = parse_expression();
		if (expr)
			add_parameter(node, expr);
	}
	else
	if (t.type == VAR)
	{
		node = parse_var();
		if (!node) ERROR_RET;
	}
	else
	if (t.type == IDENT)
	{
		// Could be assignment or function call
		NEXT_TOKEN
		cur_index -= 2; // Undo ident and LPAREN
		if (t.type == LPAREN)
			node = parse_call();
		else
		{
			node = allocate_node(ASSIGN, 0);
			Node* target = parse_lvalue();
			if (!target) ERROR_RET;
			add_child(node, target);
			EXPECT(EQ);
			Node* expr = parse_expression();
			if (!expr) ERROR_RET;
			add_child(node, expr);
		}
	}
	else
	if (t.type == WHILE)
	{
		node = allocate_node(WHILE, 0);
		Node* cond = parse_expression();
		if (!cond) ERROR_RET;
		add_parameter(node, cond);
		new_block = 1;
	}
	else
	if (t.type == IF)
	{
		node = allocate_node(IF, 0);
		Node* cond = parse_expression();
		if (!cond) ERROR_RET;
		add_parameter(node, cond);
		new_block = 1;
	}
	else
	if (t.type == END)
	{
		pop_context();
	}
	else
		ERROR_RET;
	EXPECT(EOL);
	if (node)
	{
		add_child(cur_node, node);
		if (new_block)
			push_context(parse_statement, node);
		return node;
	}
	return cur_node;
}

Node* parse_fun()
{
	Node* node = allocate_node(FUN, 0);
	Token t;
	EXPECT(IDENT);
	node->name = t.value;
	EXPECT(LPAREN);
	byte param_count = 0;
	while (1)
	{
		// Peek to see next token
		if (!get_token(cur_index, &t)) ERROR_RET;
		if (t.type == RPAREN) // No more parameters
		{
			++cur_index;
			EXPECT(EOL);
			return node;
		}
		if (param_count > 0)
		{
			if (t.type != COMMA) ERROR_RET;
			++cur_index;
		}
		Node* parameter = parse_var();
		if (!parameter) ERROR_RET;
		add_parameter(node, parameter);
		++param_count;
	}
}

Node* parse_var()
{
	Node* node = allocate_node(VAR, 0);
	Token t;
	//EXPECT(VAR);
	NEXT_TOKEN
	if (t.type == ARRAY)
	{
		node->type = ARRAY;
		NEXT_TOKEN;
		if (t.type == NUMBER)
		{
			Node* length_node = allocate_node(NUMBER, 0);
			length_node->name = t.value;
			add_parameter(node, length_node);
			NEXT_TOKEN
		}
	}
	if (t.type == BYTE || t.type == WORD || t.type == SBYTE || t.type == SWORD)
		node->var_type = t.type;
	else if (t.type == IDENT)
	{
		//node->type = ARRAY;
		node->var_type = STRUCT;
		node->type_name = t.value;
	}
	else ERROR_RET;
	EXPECT(IDENT);
	node->name = t.value;
	return node;
}

Node* parse_struct_var()
{
	Node* node=0;
	Token t;
	NEXT_TOKEN
	if (t.type == VAR)
	{
		node = parse_var();
		if (!node) ERROR_RET;
		EXPECT(EOL);
		add_child(cur_node, node);
	}
	else
	if (t.type == END)
	{
		EXPECT(EOL);
		pop_context();
	}
	return cur_node;
}

//#define ADD_CHILD(x) { Node* node=x(); if (node) add_child(cur_node, node); else { error=1; return 0; } }

Node* parse_global()
{
	Token t;
	Node* node = 0;
	NEXT_TOKEN
	if (t.type == EOL) return &root;
	if (t.type == VAR)
	{
		node = parse_var();
		if (!node) ERROR_RET;
		EXPECT(EOL);
		add_child(cur_node, node);
	}
	else if (t.type == FUN)
	{
		node = parse_fun();
		if (!node) ERROR_RET;
		add_child(&root, node);
		push_context(parse_statement, node);
	}
	else if (t.type == STRUCT)
	{
		node = allocate_node(STRUCT, 0);
		EXPECT(IDENT);
		node->name = t.value;
		EXPECT(EOL);
		add_child(&root, node);
		push_context(parse_struct_var, node);
	}
	else if (t.type == EXTERN)
	{
		EXPECT(FUN);
		node = parse_fun();
		add_child(cur_node, node);
	}
	else if (t.type == CONST)
	{
		EXPECT(IDENT);
		Constant c;
		c.name = t.value;
		EXPECT(NUMBER);
		c.value = t.value;
		EXPECT(EOL);
		vector_push(constants, &c);
	}
	else
	{ error = 1; return 0; }
	return &root;
}





void p_init(word size, token_func f)
{
	constants = vector_new(sizeof(Constant));
	error = 0;
	tokens_size = size;
	get_lex_token = f;
	init_node(&root, 0);
	root.type = ROOT;
	push_context(parse_global, &root);
	cur_node = &root;
	cur_index = 0;
}

void p_shut()
{
	release_root();
	vector_shut(constants);
}

byte p_parse()
{
	while (error == 0)
	{
		state current = context();
		if (!current()) break;
#ifdef PRINTS
		//printf("\n\n----------------------------------------------\n");
		//p_print();
#endif
	}
	return (error == 0 ? 1 : 0);
}

Node* p_root()
{
	return &root;
}

