#include <codegen.h>
#include <parser.h>
#include <vector.h>
#include <strhash.h>

#define ERROR_RET { error=1; exit(1); }
#define ASSERT(x)

#ifndef PRINTS
void exit(int);
#endif

typedef enum data_location_
{
	IMMEDIATE = 1,	// Value is known in compile time
	A=2,			// Value is a byte stored in A
	HL=3,			// Value is a word (not address), stored in HL
	STACK=4			// Value is an address (array / struct) pushed to stack
} DataLocation;

typedef struct data_type_
{
	BaseType	base_type;
	byte		local;		// 0 for absolute, 1 for relative to IX
} DataType;

typedef struct term_
{
	DataLocation	location;
	DataType		type;
	word			immediate;
} Term;

typedef struct variable_
{
	// For global variables, address is absolute
	// For local variables, address is relative to stack pointer at entry
	word		name;
	word		address;
	word		size;
	DataType	type;
} Variable;

typedef struct address_
{
	word		name;
	word		address;
} Address;

byte error=0;
Vector* variables;
Vector* knowns;
Vector* unknowns;
Node* root;
file_write_func raw_write=0;
word write_offset=0;

word get_known_address(word name)
{
	word i=0,n=vector_size(knowns);
	for (; i < n; ++i)
	{
		Address* a = (Address*)vector_access(knowns,i);
		if (a->name==name)
			return a->address + 0x1000; // Add OS size offset
	}
	ERROR_RET;
	return 0;
}

void add_known_address(word name, word addr)
{
	Address known = { name,addr };
	vector_push(knowns, &known);
}

// Add an unknown.  Later when the location of 'name' is known, 
// its value should be written to 'addr'
void add_unknown_address(word name, word addr)
{
	Address unknown = { name,addr };
	vector_push(unknowns, &unknown);
}


void write(const byte* data, word len)
{
	write_offset += raw_write(write_offset, data, len);
}

#define WRITE(x) write(x,sizeof(x))

void ld_hl_immed(word address)
{
	byte cmd[] = { 0x21, address & 255, address >> 8 };
	WRITE(cmd);
}

void write_byte(byte b) { write(&b, 1); }
#define MULTI_BYTE_CMD(name) write(name##_cmd,sizeof(name##_cmd))

#define add_c	write_byte(0x81)
#define sub_c	write_byte(0x91)
#define and_c	write_byte(0xA1)
#define or_c	write_byte(0xB1)
#define xor_c	write_byte(0xA9)
const byte lsh_c_cmd[] = { 0xCB, 0x21 };
#define lsh_c MULTI_BYTE_CMD(lsh_c)
const byte rsh_c_cmd[] = { 0xCB, 0x39 };
#define rsh_c MULTI_BYTE_CMD(rsh_c)

#define push_af		write_byte(0xF5)
#define pop_af		write_byte(0xF1)
#define push_hl		write_byte(0xE5)
#define pop_hl		write_byte(0xE1)
#define pop_bc		write_byte(0xC1)
#define ld_a_mem_hl	write_byte(0x7E)
#define add_hl_bc	write_byte(0x09)

#define ld_mem_hl_a	write_byte(0x77)
#define sub_a		write_byte(0x97)
#define set_bc_hl	push_hl; pop_bc
#define ld_mem_hl_b write_byte(0x70)
#define ld_mem_hl_c write_byte(0x71)
#define inc_hl		write_byte(0x23)
void ld_a_mem_ix(byte offset) { const byte cmd[] = {0xDD, 0x7E, offset}; WRITE(cmd); }
void ld_h_mem_ix(byte offset) { const byte cmd[] = { 0xDD, 0x66, offset }; WRITE(cmd); }
void ld_l_mem_ix(byte offset) { const byte cmd[] = { 0xDD, 0x6E, offset }; WRITE(cmd); }
void ld_a_mem_immed(word addr) { const byte cmd[] = { 0x3A, (addr & 0xFF), (addr >> 8) }; WRITE(cmd); }
void ld_hl_mem_immed(word addr) { const byte cmd[] = { 0x2A, (addr & 0xFF), (addr >> 8) }; WRITE(cmd); }

const byte sub_hl_bc_cmd[] = { 0xBF, 0xED, 0x42 };  //  Clear-carry,  SBC HL,BC
#define sub_hl_bc MULTI_BYTE_CMD(sub_hl_bc)

const byte ld_bc_mem_hl_cmd[] = { 0x4E, 0x23, 0x46 }; // LD C,(HL)   INC HL    LD B,(HL)
#define ld_bc_mem_hl MULTI_BYTE_CMD(ld_bc_mem_hl)
const byte set_hl_bc_cmd[] = { 0xC5, 0xE1 }; // PUSH BC    POP HL
#define set_hl_bc MULTI_BYTE_CMD(set_hl_bc)
const byte set_hl_a_cmd[] = { 0x6F, 0x26, 0x00 }; // LD L,A    LD H,#0
#define set_hl_a MULTI_BYTE_CMD(set_hl_a)

#define set_l_a write_byte(0x6F)
#define set_a_l write_byte(0x7D)
void set_a_immed(byte b) { byte cmd[] = { 0x3E, b }; WRITE(cmd); }
void set_h_immed(byte b) { byte cmd[] = { 0x26, b }; WRITE(cmd); }
void set_b_immed(byte b) { byte cmd[] = { 0x06, b }; WRITE(cmd); }
void set_bc_immed(word w) { byte cmd[] = { 0x01, (w & 0xFF), (w >> 8) }; WRITE(cmd); }
void set_de_immed(word w) { byte cmd[] = { 0x11, (w & 0xFF), (w >> 8) }; WRITE(cmd); }
void set_hl_immed(word w) { byte cmd[] = { 0x21, (w & 0xFF), (w >> 8) }; WRITE(cmd); }

byte is_binary_operator(byte type)
{
	switch (type)
	{
	case PLUS:
	case MINUS:
	case LSH:
	case RSH:
	case AMP:
	case PIPE:
	case CARET:
		return 1;
	}
	return 0;
}

void print_name(word id);
word struct_size(word name);

word type_size(BaseType* base_type)
{
	if (base_type->sub_type == STRUCT) return struct_size(base_type->type_name);
	ASSERT(base_type->sub_type == PRIMITIVE);
	switch (base_type->type_name)
	{
	case SBYTE:
	case BYTE: return 1;
	case SWORD:
	case WORD: return 2;
	default:
		ERROR_RET;
	}
	return 0;
}

word calculate_struct_size(Node* node)
{
	Node* child = node->child;
	word sum = 0;
	while (child)
	{
		sum += type_size(&child->data_type);
		child = child->sibling;
	}
	return sum;
}

word struct_size(word name)
{
	Node* child = root->child;
	while (child)
	{
		if (child->type == STRUCT && child->name == name)
		{
			return calculate_struct_size(child);
		}
		child = child->sibling;
	}
	error = 1;
	return 0;
}

// Given a struct name and field name, calculate the relative term.
// If successful, returns an immediate term relative to the start of the struct
void struct_field_offset(word struct_name, word field_name, Term* res)
{
	res->location = IMMEDIATE;
	res->immediate = 0;
	Node* child = root->child;
	while (child)
	{
		if (child->type == STRUCT && child->name == struct_name)
		{
			child = child->child;
			word offset = 0;
			while (child)
			{
				if (child->name == field_name)
				{
					res->immediate = offset;
					res->location = IMMEDIATE;
					res->type.base_type = child->data_type;
					res->type.local = 99; // Should be ignored
					return;
				}
				offset += type_size(&child->data_type);
				child = child->sibling;
			}
			ERROR_RET;
			return;
		}
		child = child->sibling;
	}
	ERROR_RET;
}

word var_size(Node* node)
{
	if (node->data_type.type == VAR)
		return type_size(&node->data_type);
	else
	if (node->data_type.type == ARRAY && node->parameters)
	{
		return node->parameters->name * type_size(&node->data_type);
	}
	else ERROR_RET;
	return 0;
}

word local_var_size(Node* node)
{
	if (!node) return 0;
	if (node->type == VAR)
		return var_size(node);
	word sum = 0;
	Node* child = node->child;
	while (child)
	{
		if (child->type == VAR)
			sum += var_size(child);
		if (child->type==WHILE || child->type==IF)
			sum += local_var_size(child);
		child = child->sibling;
	}
	return sum;
}

void scan_parameters(Node* fun)
{
	if (!fun) return;
	Variable var;
	Node* param = fun->parameters;
	word offset = 0;
	while (param)
	{
		offset += 2;
		var.name = param->name;
		var.address = offset;
		var.size = 2;
		var.type.base_type = param->data_type;
		var.type.local = 1;
		vector_push(variables, &var);
		param = param->sibling;
	}
}

// Accepts a block node (root / fun / while / if) and a variables vector.
// Adds local variables to the vector and returns the total size of the variables
word scan_variables(Node* node, word offset, byte local)
{
	if (!node) return 0;
	Variable var;
	word sum = 0;
	Node* child = node->child;
	if (!local) offset = 0x1003; // start of global vars
	while (child)
	{
		if (child->type == VAR)
		{
			var.name = child->name;
			var.type.local = local;
			var.size = var_size(child);
			var.type.base_type = child->data_type;
			if (local)
			{
				offset -= var.size;
				var.address = offset;
			}
			else
			{
				var.address = offset;
				offset += var.size;
			}
			vector_push(variables, &var);
			sum += var_size(child);
		}
		else
		if (child->type == WHILE || child->type == IF)
			sum += scan_variables(child, offset, 1);
		child = child->sibling;
	}
	return sum;
}

byte find_variable(word name, Variable* var)
{
	word n = vector_size(variables);
	for (word i = 0; i < n; ++i)
	{
		vector_get(variables, i, var);
		if (var->name == name) return 1;
	}
	return 0;
}

void set_prim_type(BaseType* t, byte type_name)
{
	t->type = VAR;
	t->sub_type = PRIMITIVE;
	t->type = type_name;
}

void get_node_address(Node* node, Term*);

void calculate_expression(Node* node, Term* res)
{
	if (node->type == NUMBER)
	{
		res->location = IMMEDIATE;
		res->immediate = node->name;
		res->type.local = 0;
	}
	else if (node->type == IDENT)
	{
		Variable var;
		if (find_variable(node->name, &var))
		{
			res->type = var.type;
			word size=type_size(&var.type.base_type);
			if (var.type.local)
			{
				if (size == 1)
				{
					ld_a_mem_ix(var.address);
					res->location=A;
				}
				else
				{
					ld_l_mem_ix(var.address);
					ld_h_mem_ix(var.address+1);
					res->location = HL;
				}
			}
			else
			{
				if (size == 1)
				{
					ld_a_mem_immed(var.address);
					res->location=A;
				}
				else
				{
					ld_hl_mem_immed(var.address);
					res->location=HL;
				}
			}
		}
	}
	else if (node->type == DOT || node->type == INDEX)
	{
		get_node_address(node, res);
		pop_hl;
		word size = type_size(&res->type.base_type);
		if (size == 1)
		{
			ld_a_mem_hl;
			res->location = A;
			res->type.local = 0;
		}
		else if (size == 2)
		{
			ld_bc_mem_hl;
			set_hl_bc;
			res->location = HL;
			res->type.local = 0;
		}
		else ERROR_RET;
	}
	else if (is_binary_operator(node->type))
	{
		if (!node->child || !node->child->sibling) ERROR_RET;
		Term left,right;
		calculate_expression(node->child,&left);
		switch (left.location)
		{
		case A:		push_af; pop_bc; set_b_immed(0); break;
		case HL:	push_hl; pop_bc; break;
		case IMMEDIATE: set_bc_immed(left.immediate); break;
		default: ERROR_RET;
		}
		calculate_expression(node->child->sibling,&right);
		word left_size = 1, right_size = 1;
		if (left.location!=IMMEDIATE) 
			left_size = type_size(&left.type.base_type);
		if (right.location != IMMEDIATE)
			right_size = type_size(&right.type.base_type);
		word max_size = (left_size > 1 || right_size > 1) ? 2 : 1;
		switch (right.location)
		{
		case A:  if (max_size > 1) { push_af; pop_hl; set_h_immed(0); } break;
		case HL: break;
		case IMMEDIATE: ld_hl_immed(right.immediate); break;
		default:
			ERROR_RET;
		}
		if (max_size > 1)
		{
			switch (node->type)
			{
			case PLUS: add_hl_bc; res->location = HL; set_prim_type(&res->type.base_type, WORD); break;
			case MINUS: sub_hl_bc; res->location = HL; set_prim_type(&res->type.base_type, WORD); break;
			default: ERROR_RET;
			}
		}
		else
		{
			switch (node->type)
			{
			case PLUS: add_c; break;
			case MINUS: sub_c; break;
			case LSH: lsh_c; break;
			case RSH: rsh_c; break;
			case AMP: and_c; break;
			case PIPE: or_c; break;
			case CARET: xor_c; break;
			default: ERROR_RET;
			}
			res->location = A;
			set_prim_type(&res->type.base_type, BYTE);
		}
	}
	else ERROR_RET;
}

void multiply_hl(word m)
{
	set_bc_hl;
	set_de_immed(m);
	word addr = get_known_address(sh_get("mult_bc_de"));
	const byte cmd[] = { 0xCD, (addr&0xFF), (addr>>8)};
	WRITE(cmd);
}

// Input:  node of address to evaluate
// Outputs:
//		Term - Location on STACK
void get_node_address(Node* node, Term* res)
{
	res->location = STACK;
	res->immediate = 0;
	if (node->type == IDENT)
	{
		Variable var;
		if (find_variable(node->name, &var))
		{
			res->type = var.type;
			ld_hl_immed(var.address);
			push_hl;
		}
		else
		ERROR_RET;
	}
	else if (node->type == INDEX)
	{
		Term array_address;
		get_node_address(node->child,&array_address);
		if (array_address.type.base_type.type != ARRAY) ERROR_RET;
		word elem_size = type_size(&array_address.type.base_type);
		Term index;
		calculate_expression(node->child->sibling,&index);
		if (index.location == IMMEDIATE)
		{
			set_hl_immed(index.immediate * elem_size);
		}
		else
		{
			switch (index.location)
			{
			case A:				set_hl_a; break;
			case HL:			break;
			case STACK:
			default:	ERROR_RET;
			}
			multiply_hl(elem_size);
		}
		pop_bc; // Get the array address from the stack
		add_hl_bc; // Add the index
		push_hl;
		res->type.base_type = array_address.type.base_type;
		res->type.local = 0; // No optimization yet.  TBD
	}
	else if (node->type == DOT)
	{
		Term struct_addr;
		get_node_address(node->child,&struct_addr);
		if (struct_addr.type.base_type.sub_type != STRUCT) ERROR_RET;
		Node* field_node = node->child->sibling;
		Term field;
		struct_field_offset(struct_addr.type.base_type.type_name, field_node->name, &field);
		if (field.location != IMMEDIATE) ERROR_RET;
		res->type.local = 0;
		res->type.base_type = field.type.base_type;
		pop_bc; // struct base address
		ld_hl_immed(field.immediate); // offset
		add_hl_bc; // base+offset
		push_hl;
	}
	else ERROR_RET;
}

void generate_statement(Node* statement);
void generate_block(Node* block);

void generate_call(Node* node)
{
	Node* p=node->parameters;
	while (p)
	{
		Term res;
		if (p->data_type.type == ARRAY || p->data_type.sub_type == STRUCT)
		{
			get_node_address(p,&res);
			if (res.location!=STACK) ERROR_RET;
		}
		else
		{
			calculate_expression(p,&res);
			switch (res.location)
			{
			case IMMEDIATE:
				set_hl_immed(res.immediate);
				break;
			case HL: break;
			case A:	 set_hl_a;
			default: ERROR_RET;
			}
			push_hl;
		}
		p=p->sibling;
	}
	add_unknown_address(node->name, write_offset+1);
	const byte cmd[] = {0xCD, 0x00, 0x00};
	WRITE(cmd);
}

void generate_assignment(Node* node)
{
	Node* target_node = node->child;
	Term target_term;
	get_node_address(target_node,&target_term);
	word target_size = type_size(&target_term.type.base_type);
	Node* expr_node = target_node->sibling;
	
	Term source_term;
	calculate_expression(expr_node,&source_term);
	switch (source_term.location)
	{
	case A:
		pop_hl;
		ld_mem_hl_a;
		if (target_size > 1)
		{ 
			inc_hl;
			sub_a; 
			ld_mem_hl_a;
		}
		break;
	case HL:
		set_bc_hl;
		pop_hl;
		ld_mem_hl_c;
		if (target_size > 1)
		{
			inc_hl;
			ld_mem_hl_b;
		}
		break;
	default: ERROR_RET;
	}
}

byte generate_condition(Node* node)
{
	if (node->type == PIPE || node->type == AMP)
	{
		byte b=generate_condition(node->child);
	}
	else
	{
		if (node->type==LPAREN) generate_condition(node->child);
		else
		{
			byte swap = (node->type == GT || node->type == LE);
			if (node->type == LT || node->type == GT || node->type == LE ||
				node->type == GE || node->type == EQ || node->type == NE)
			{
				Term left,right;
				calculate_expression(node->child, &left);
				switch (left.location)
				{
				case IMMEDIATE: set_a_immed(left.immediate);
				case A: push_af; break;
				case HL: set_h_immed(0); push_hl; break;
				default:
					ERROR_RET;
				}
				calculate_expression(node->child->sibling, &right);
				switch (right.location)
				{
				case IMMEDIATE: 
					if (swap) set_a_immed(right.immediate);
					else set_hl_immed(right.immediate); 
					break;
				case A: if (!swap) set_l_a; break;
				case HL: if (swap) set_a_l; break;
				default:
					ERROR_RET;
				}
			}
			else ERROR_RET;
			if (swap) pop_hl;
			else pop_af;
			write_byte(0xBD); // CP L
			switch (node->type)
			{
			case LT: return 0x38; // JR C
			case GT: return 0x38; // JR C
			case LE: return 0x30; // JR NC
			case GE: return 0x30; // JR NC
			case EQ: return 0x28; // JR Z
			case NE: return 0x20; // JR NZ
			default: ERROR_RET;
			}
		}
	}
	return 0;
}

void generate_cond_block(Node* node, byte loop)
{
	if (!node->parameters) ERROR_RET;
	word start_addr = write_offset+0x1000;
	byte jump = generate_condition(node->parameters);
	word end_of_block = sh_temp();
	add_unknown_address(end_of_block, write_offset + 3);
	const byte cmd[] = { jump, 0x03, 0xC3, 0x00, 0x00 };
	WRITE(cmd);
	generate_block(node);
	if (loop)
	{
		const byte jump_back[] = { 0xC3, (start_addr & 0xFF), (start_addr >> 8) };
		WRITE(jump_back);
	}
	add_known_address(end_of_block, write_offset);
}

void generate_statement(Node* statement)
{
	if (statement->type == ASSIGN) generate_assignment(statement);
	else if (statement->type == WHILE) generate_cond_block(statement,1);
	else if (statement->type == IF) generate_cond_block(statement,0);
	else if (statement->type == CALL) generate_call(statement);
}

void generate_block(Node* block)
{
	Node* child = block->child;
	while (child)
	{
		generate_statement(child);
		child = child->sibling;
	}
}

// Accepts node of funuction, memory location of the function (offset) and size of local variables
// Returns the offset beyond the function
void generate_function(Node* func, word locals_size)
{
	add_known_address(func->name,write_offset);
	word neg_locals = -locals_size;
	byte init_stack[] = {
		0xDD, 0xE5,								// PUSH IX
		0xDD,0x21,0x00,0x00,					// LD IX,#0
		0xDD,0x39,								// ADD IX,SP
		0x21,(neg_locals&255),(neg_locals>>8),	// LD HL,-locals_size
		0x39,									// ADD HL,SP
		0xF9									// LD SP,HL
	};
	byte close_stack[] = {
		0xDD, 0xF9,								// LD SP,IX
		0xDD, 0xE1,								// POP IX
		0xC9									// RET
	};
	WRITE(init_stack);
	generate_block(func);
	WRITE(close_stack);
}

#ifdef PRINTS
#include <stdio.h>
void scan_sizes(Node* root)
{
	printf("ROOT ");
	printf(" %hd\n", local_var_size(root));
	Node* node = root->child;
	while (node)
	{
		//word size = local_var_size(node, 0);
		print_name(node->name);
		printf(" %hd\n",local_var_size(node));
		node = node->sibling;
	}
}
#endif

void generate_common_functions()
{
	Address address;
	address.name = sh_get("mult_bc_de");
	address.address = write_offset;
	vector_push(knowns, &address);
	const byte mult_code[] = {	0x21,0x00,0x00,0x78,0x06,0x10,0x29,0xCB,
								0x21,0x17,0x30,0x01,0x19,0x10,0xF7,0xC9 };
	WRITE(mult_code);
}

void fill_unknowns()
{
	word i=0,n=vector_size(unknowns);
	for (; i < n; ++i)
	{
		Address* unk=(Address*)vector_access(unknowns,i);
		word addr=get_known_address(unk->name);
		raw_write(unk->address,(byte*)&addr,2);
	}
}

byte generate_code(Node* proot, file_write_func fwf)
{
	root = proot;
	raw_write = fwf;
	word globals_size = scan_variables(root, 0, 0);
	word globals_count = vector_size(variables);
	byte header[] = { 0xC3, 0x00, 0x00 };
	WRITE(header);
	add_unknown_address(sh_get("main"), 1);
	for (word i = 0; i < globals_size; ++i)		// Reserve room for globals
		write_byte(0);
	generate_common_functions();
	Node* child = root->child;
	while (child)
	{
		if (child->type == FUN && child->child) // Not an extern
		{
			scan_parameters(child);
			word locals_size = scan_variables(child, 0, 1);
			generate_function(child, locals_size);
			vector_resize(variables, globals_count); // Remove local vars
		}
		child = child->sibling;
	}
	fill_unknowns();
	return 1;
}

void gen_init()
{
	write_offset = 0;
	variables = vector_new(sizeof(Variable));
	knowns = vector_new(sizeof(Address));
	unknowns = vector_new(sizeof(Address));
}

void gen_shut()
{
	vector_shut(unknowns);
	vector_shut(knowns);
	vector_shut(variables);
}

