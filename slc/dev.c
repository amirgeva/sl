#include "dev.h"
#include "datastr/strhash.h"

extern StrHash* texts;

#ifdef DEV

#include <stdio.h>
#include <stdlib.h>

FILE* output=0;

void print_expression(Node* node);
void print_call(Node* node);

void print_indent(int indent)
{
	for (int i = 0; i < indent; ++i)
		fprintf(output," ");
}

void print_type(byte var_type)
{
	switch (var_type)
	{
	case BYTE: fprintf(output,"byte"); break;
	case SBYTE: fprintf(output,"sbyte"); break;
	case WORD: fprintf(output,"word"); break;
	case SWORD: fprintf(output,"sword"); break;
	default:
		fprintf(output,"Unknown");
		exit(1);
	}
}

void print_name(word id)
{
	char name[20];
	name[16] = 0;
	sh_text(texts, name, id);
	fprintf(output,"%s", name);
}

void print_array(Node* node)
{
	int size = 0;
	if (node->parameters)
	{
		size = node->parameters->name;
	}
	fprintf(output,"array ");
	if (size > 0)
		fprintf(output,"%d ", size);
	if (node->data_type.sub_type==STRUCT)
		print_name(node->data_type.type_name);
	else
		print_type(node->data_type.type_name);
	fprintf(output," ");
	print_name(node->name);
}

void print_var(Node* node)
{
	if (node->data_type.sub_type==STRUCT)
		print_name(node->data_type.type_name);
	else
		print_type(node->data_type.type_name);
	fprintf(output," ");
	print_name(node->name);
}

void print_fun(Node* node)
{
	fprintf(output,"fun ");
	print_name(node->name);
	fprintf(output,"(");
	Node* parameter = node->parameters;
	while (parameter)
	{
		switch (parameter->data_type.type)
		{
		case VAR: print_var(parameter); break;
		case ARRAY: print_array(parameter); break;
		}
		parameter = parameter->sibling;
		if (parameter) fprintf(output,", ");
	}
	fprintf(output,")\n");
}

void print_cond(const char* title, Node* node)
{
	fprintf(output,"%s ", title);
	print_expression(node->parameters);
	fprintf(output,"\n");
}

void print_value(Node* node)
{
	if (node->type == NUMBER) fprintf(output,"%d", node->name);
	else
		if (node->type == DOT)
		{
			print_value(node->child);
			fprintf(output,".");
			print_value(node->child->sibling);
		}
		else if (node->type == INDEX)
		{
			print_value(node->child);
			fprintf(output,"[");
			print_expression(node->child->sibling);
			fprintf(output,"]");
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
	if (node->type == LPAREN)
	{
		fprintf(output,"(");
		print_expression(node->child);
		fprintf(output,")");
		return;
	}
	switch (node->type)
	{
#define PRINT_OPER(x,y) case x: print_expression(node->child);\
					fprintf(output,#y); print_expression(node->child->sibling); break
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
	default:
		print_value(node);
	}
}


void print_assign(Node* node)
{
	Node* value = node->child;
	if (value)
	{
		print_value(value);
		value = value->sibling;
	}
	fprintf(output,"=");
	if (value)
		print_expression(value);
	fprintf(output,"\n");
}

void print_call(Node* node)
{
	print_name(node->name);
	fprintf(output,"(");
	Node* parameter = node->parameters;
	while (parameter)
	{
		print_expression(parameter);
		parameter = parameter->sibling;
		if (parameter) fprintf(output,",");
	}
	fprintf(output,")\n");
}

void print_base_type(BaseType* base_type, Node* parameters)
{
	if (base_type->type == ARRAY)
	{
		fprintf(output,"array ");
		if (parameters) fprintf(output,"%hd ",parameters->name);
	}
	if (base_type->sub_type==STRUCT) print_name(base_type->type_name);
	else print_type(base_type->type_name);
}

void dev_print_tree_node(Node* node, int indent)
{
	if (!node) return;
	switch (node->type)
	{
	case ROOT:	print_indent(indent); fprintf(output,"ROOT\n"); break;
	case FUN:	print_indent(indent); print_fun(node); break;
	case STRUCT:print_indent(indent); fprintf(output,"struct "); print_name(node->name); fprintf(output,"\n"); break;
	case VAR:	print_indent(indent); fprintf(output,"var "); print_base_type(&node->data_type, node->parameters); fprintf(output," "); print_name(node->name); fprintf(output,"\n"); break;
	case ASSIGN:print_indent(indent); print_assign(node); break;
	case WHILE: print_indent(indent); print_cond("while", node); break;
	case IF:
	case IFELSE:print_indent(indent); print_cond("if", node); break;
	case CALL:	print_indent(indent); print_call(node); break;

	case PLUS:		print_indent(indent); fprintf(output,"+\n"); break;
	case LSH:		print_indent(indent); fprintf(output, "<<\n"); break;
	case LPAREN:	print_indent(indent); fprintf(output, "()\n"); break;
	case NUMBER:	print_indent(indent); fprintf(output, "%hd\n", node->name); break;
	case IDENT:		print_indent(indent); print_name(node->name); fprintf(output, "\n"); break;
	//case PLUS:  print_indent(indent); fprintf(output, "+"); break;
	}
}

void print_tree_nodes(Node* node, int indent)
{
	while (node)
	{
		dev_print_tree_node(node, indent);
		if (node->type == IFELSE)
		{
			print_tree_nodes(node->child->child, indent+2);
			print_indent(indent);
			fprintf(output,"else\n");
			print_tree_nodes(node->child->sibling->child, indent + 2);
			print_indent(indent);
			fprintf(output,"end\n");
		}
		else
		{
			print_tree_nodes(node->child, indent + 2);
			if (node->type == FUN || node->type == IF ||
				node->type == WHILE || node->type == STRUCT)
			{
				print_indent(indent);
				fprintf(output,"end\n");
			}
		}
		node = node->sibling;
	}
}

void dev_print_tree(Node* root)
{
	print_tree_nodes(root, 0);
}

void dev_init()
{
	output=stdout;
}

void dev_shut()
{
}

void dev_output(void* f)
{
	output=(FILE*)f;
}

#else // DEV  (development build vs platform build
void print_tree(Node* node) { (void*)node; }
void dev_init() {}
void dev_shut() {}
void dev_output(void* f) { (void*)f; }
#endif
