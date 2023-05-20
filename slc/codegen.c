#include "codegen.h"
#include "parser.h"

//word _mulint(word a, word b);

struct Variable
{
	// For global variables, address is absolute
	// For local variables, address is relative to stack pointer at entry
	word address;
	word size;
	byte local;
};

void print_name(word id);

word type_size(byte var_type)
{
	switch (var_type)
	{
	case SBYTE:
	case BYTE: return 1;
	case SWORD:
	case WORD: return 2;
	}
	return 0;
}

word var_size(Node* node)
{
	if (node->type == VAR)
		return type_size(node->var_type);
	if (node->type == ARRAY && node->parameters)
		return node->parameters->name * type_size(node->var_type);
	return 0;
}

word local_var_size(Node* node, int indent)
{
	if (!node) return 0;
	if (node->type == VAR || node->type == ARRAY)
		return var_size(node);
	//print_tree_node(node, indent);
	word sum = 0;
	Node* child = node->child;
	while (child)
	{
		if (child->type == VAR || child->type==ARRAY)
			sum += var_size(child);
		if (child->type==WHILE || child->type==IF)
			sum += local_var_size(child, indent+2);
		child = child->sibling;
	}
	return sum;
}


#ifdef PRINTS
#include <stdio.h>
void scan_sizes(Node* root)
{
	printf("ROOT ");
	printf(" %hd\n", local_var_size(root, 0));
	Node* node = root->child;
	while (node)
	{
		//word size = local_var_size(node, 0);
		print_name(node->name);
		printf(" %hd\n",local_var_size(node, 0));
		node = node->sibling;
	}
}
#endif