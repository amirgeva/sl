#include "dbg.h"


#ifdef PRINTS

#include <stdio.h>

void print_expression(Node* node);
void print_call(Node* node);

void print_indent(int indent)
{
	for (int i = 0; i < indent; ++i)
		printf(" ");
}

void print_type(byte var_type)
{
	switch (var_type)
	{
	case BYTE: printf("byte"); break;
	case SBYTE: printf("sbyte"); break;
	case WORD: printf("word"); break;
	case SWORD: printf("sword"); break;
	default:
		printf("Unknown");
		exit(1);
	}
}

void print_name(word id)
{
	char name[20];
	name[16] = 0;
	sh_text(name, id);
	printf("%s", name);
}

void print_array(Node* node)
{
	int size = 0;
	if (node->parameters)
	{
		size = node->parameters->name;
	}
	printf("array ");
	if (size > 0)
		printf("%d ", size);
	print_name(node->data_type.type_name);
	printf(" ");
	print_name(node->name);
}

void print_var(Node* node)
{
	print_name(node->data_type.type_name);
	printf(" ");
	print_name(node->name);
}

void print_fun(Node* node)
{
	printf("fun ");
	print_name(node->name);
	printf("(");
	Node* parameter = node->parameters;
	while (parameter)
	{
		switch (parameter->type)
		{
		case VAR: print_var(parameter); break;
		case ARRAY: print_array(parameter); break;
		}
		parameter = parameter->sibling;
		if (parameter) printf(", ");
	}
	printf(")");
}

void print_cond(const char* title, Node* node)
{
	printf("%s ", title);
	print_expression(node->parameters);
	printf("\n");
}

void print_value(Node* node)
{
	if (node->type == NUMBER) printf("%d", node->name);
	else
		if (node->type == DOT)
		{
			print_value(node->child);
			printf(".");
			print_value(node->child->sibling);
		}
		else if (node->type == INDEX)
		{
			print_value(node->child);
			printf("[");
			print_expression(node->child->sibling);
			printf("]");
		}
		else if (node->type == IDENT)
		{
			print_name(node->name);
		}
		else if (node->type == CALL)
		{
			print_call(node);
		}
}

void print_expression(Node* node)
{
	if (node->child)
		print_value(node->child);
	else
	{
		print_value(node);
		return;
	}
	switch (node->type)
	{
#define PRINT_OPER(x,y) case x: printf(#y); break
		PRINT_OPER(PLUS, +);
		PRINT_OPER(MINUS, -);
		PRINT_OPER(EQ, =);
		PRINT_OPER(LSH, << );
		PRINT_OPER(RSH, >> );
		PRINT_OPER(LT, < );
		PRINT_OPER(GT, > );
		PRINT_OPER(LE, <= );
		PRINT_OPER(GE, >= );
		PRINT_OPER(AMP, &);
		PRINT_OPER(PIPE, | );
		PRINT_OPER(CARET, ^);
	}
	print_expression(node->child->sibling);
}


void print_assign(Node* node)
{
	Node* value = node->child;
	if (value)
	{
		print_value(value);
		value = value->sibling;
	}
	printf("=");
	if (value)
		print_expression(value);
	printf("\n");
}

void print_call(Node* node)
{
	print_name(node->name);
	printf("(");
	Node* parameter = node->parameters;
	while (parameter)
	{
		print_expression(parameter);
		parameter = parameter->sibling;
		if (parameter) printf(",");
	}
	printf(")");
}

void print_self(Node* node)
{
	switch (node->type)
	{
	case 0: return;
	case VAR: printf("var "); print_var(node); break;
	case ARRAY: printf("var "); print_array(node); break;
	case FUN: print_fun(node); break;
	case STRUCT: printf("struct "); print_name(node->name); break;
	case IF: print_cond("if", node); break;
	case WHILE: print_cond("while", node); break;
	case ASSIGN: print_assign(node); break;
	case CALL: print_call(node); break;
	case RETURN: printf("return "); print_expression(node->parameters); break;
	default:
		printf("Unknown\n"); exit(1);
	}
	printf("\n");
}

void print_code_tree(Node* node, int indent)
{
	if (!node) return;
	print_indent(indent);
	print_self(node);
	if (node->child && (node->type == FUN || node->type == IF ||
						node->type == WHILE || node->type == STRUCT || node->type == ROOT))
	{
		print_code_tree(node->child, indent + 2);
		if (node->type != ROOT)
		{
			print_indent(indent);
			printf("end\n");
		}
	}
	if (node->sibling)
		print_code_tree(node->sibling, indent);
}

void print_base_type(BaseType* base_type, Node* parameters)
{
	if (base_type->type == ARRAY)
	{
		printf("array ");
		if (parameters) printf("%hd ",parameters->name);
	}
	if (base_type->sub_type==STRUCT) print_name(base_type->type_name);
	else print_type(base_type->type_name);
}

void print_tree_node(Node* node, int indent)
{
	if (!node) return;
	switch (node->type)
	{
	case ROOT:	print_indent(indent); printf("ROOT\n"); break;
	case FUN:	print_indent(indent); printf("FUN "); print_name(node->name); printf("\n"); break;
	case STRUCT:print_indent(indent); printf("STRUCT "); print_name(node->name); printf("\n"); break;
	case VAR:	print_indent(indent); printf("VAR "); print_base_type(&node->data_type, node->parameters); printf(" "); print_name(node->name); printf("\n"); break;
	case ASSIGN:print_indent(indent); print_assign(node); break;
	case WHILE: print_indent(indent); print_cond("WHILE", node); break;
	case IF:	print_indent(indent); print_cond("IF", node); break;
	}
}

void print_tree_nodes(Node* node, int indent)
{
	while (node)
	{
		print_tree_node(node, indent);
		print_tree_nodes(node->child, indent + 2);
		node = node->sibling;
	}
}

void print_tree(Node* root)
{
	print_tree_nodes(root, 0);
}


#else
void print_code_tree(Node* node, int indent)
{
	(void*)node;
	indent;
}
#endif



void print_code(Node* root)
{
	print_code_tree(root->child, 0);
}

