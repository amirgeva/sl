#include <codegen.h>
#include <parser.h>
#include <vector.h>
#include <strhash.h>

#define ERROR_RET(line) { error=1; error_exit(line,1); }
#define ASSERT(x)

#ifndef DEV
void exit(int rc) { (void)rc; }
#endif

void error_exit(word line, int rc)
{
#ifdef DEV
	printf("Error in line %d\n",line);
#endif
	exit(rc);
}

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

typedef struct field_
{
	BaseType	type;
	word		name;
} Field;

typedef struct struct_
{
	word	name;
	Vector* fields;
} Struct;

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

static byte error=0;
static Vector* structs;
static Vector* variables;
static Vector* knowns;
static Vector* unknowns;
static parse_node_func parse_node;
static file_write_func raw_write=0;
static word write_offset=0;

word get_known_address(word line, word name)
{
	word i=0,n=vector_size(knowns);
	for (; i < n; ++i)
	{
		Address* a = (Address*)vector_access(knowns,i);
		if (a->name==name)
			return a->address + 0x1000; // Add OS size offset
	}
	ERROR_RET(line);
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
#define inc_sp		write_byte(0x33)
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
word struct_size(word line, word name);

word type_size(word line, BaseType* base_type)
{
	if (base_type->sub_type == STRUCT) return struct_size(line, base_type->type_name);
	ASSERT(base_type->sub_type == PRIMITIVE);
	switch (base_type->type_name)
	{
	case SBYTE:
	case BYTE: return 1;
	case SWORD:
	case WORD: return 2;
	default:
		ERROR_RET(line);
	}
	return 0;
}

word calculate_struct_size(word line, Struct* s)
{
	word n=vector_size(s->fields);
	word sum = 0;
	for (word i = 0; i < n; ++i)
	{
		Field* field = (Field*)vector_access(s->fields, i);
		sum+=type_size(line, &field->type);
	}
	return sum;
}

Struct* find_struct(word line, word name)
{
	word n = vector_size(structs);
	for (word i = 0; i < n; ++i)
	{
		Struct* s = (Struct*)vector_access(structs, i);
		if (s->name == name) return s;
	}
	ERROR_RET(line);
	return 0;
}

word struct_size(word line, word name)
{
	return calculate_struct_size(line, find_struct(line, name));
}

// Given a struct name and field name, calculate the relative term.
// If successful, returns an immediate term relative to the start of the struct
void struct_field_offset(word line, word struct_name, word field_name, Term* res)
{
	res->location = IMMEDIATE;
	res->immediate = 0;
	Struct* s=find_struct(line, struct_name);
	word offset = 0;
	word n=vector_size(s->fields);
	for (word i = 0; i < n; ++i)
	{
		Field* field = (Field*)vector_access(s->fields, i);
		if (field->name == field_name)
		{
			res->immediate=offset;
			res->type.base_type = field->type;
			break;
		}
		offset+= type_size(line, &field->type);
	}
}

word var_size(Node* node)
{
	if (node->data_type.type == VAR)
		return type_size(node->line, &node->data_type);
	else
	if (node->data_type.type == ARRAY && node->parameters)
	{
		return node->parameters->name * type_size(node->line, &node->data_type);
	}
	else ERROR_RET(node->line);
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
		if (child->type == WHILE || child->type == IF || child->type == IFELSE || child->type==BLOCK)
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
			word size=type_size(node->line, &var.type.base_type);
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
		word size = type_size(node->line, &res->type.base_type);
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
		else ERROR_RET(node->line);
	}
	else if (is_binary_operator(node->type))
	{
		if (!node->child || !node->child->sibling) ERROR_RET(node->line);
		Term left,right;
		calculate_expression(node->child,&left);
		switch (left.location)
		{
		case A:		push_af; pop_bc; set_b_immed(0); break;
		case HL:	push_hl; pop_bc; break;
		case IMMEDIATE: set_bc_immed(left.immediate); break;
		default: ERROR_RET(node->line);
		}
		calculate_expression(node->child->sibling,&right);
		word left_size = 1, right_size = 1;
		if (left.location!=IMMEDIATE) 
			left_size = type_size(node->line, &left.type.base_type);
		if (right.location != IMMEDIATE)
			right_size = type_size(node->line, &right.type.base_type);
		word max_size = (left_size > 1 || right_size > 1) ? 2 : 1;
		switch (right.location)
		{
		case A:  if (max_size > 1) { push_af; pop_hl; set_h_immed(0); } break;
		case HL: break;
		case IMMEDIATE: ld_hl_immed(right.immediate); break;
		default:
			ERROR_RET(node->line);
		}
		if (max_size > 1)
		{
			switch (node->type)
			{
			case PLUS: add_hl_bc; res->location = HL; set_prim_type(&res->type.base_type, WORD); break;
			case MINUS: sub_hl_bc; res->location = HL; set_prim_type(&res->type.base_type, WORD); break;
			default: ERROR_RET(node->line);
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
			default: ERROR_RET(node->line);
			}
			res->location = A;
			set_prim_type(&res->type.base_type, BYTE);
		}
	}
	else ERROR_RET(node->line);
}

void multiply_hl(word line, word m)
{
	set_bc_hl;
	set_de_immed(m);
	word addr = get_known_address(line, sh_get("mult_bc_de"));
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
			if (var.type.local && var.address<0x100 && 
				(var.type.base_type.type == ARRAY || var.type.base_type.sub_type == STRUCT))
			{
				// variable is a local parameter pointer.  Load its actual address
				ld_l_mem_ix(var.address);
				ld_h_mem_ix(var.address+1);
			}
			else
				ld_hl_immed(var.address);
			push_hl;
		}
		else
		ERROR_RET(node->line);
	}
	else if (node->type == INDEX)
	{
		Term array_address;
		get_node_address(node->child,&array_address);
		if (array_address.type.base_type.type != ARRAY) ERROR_RET(node->line);
		word elem_size = type_size(node->line, &array_address.type.base_type);
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
			default:	ERROR_RET(node->line);
			}
			multiply_hl(node->line, elem_size);
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
		if (struct_addr.type.base_type.sub_type != STRUCT) ERROR_RET(node->line);
		Node* field_node = node->child->sibling;
		Term field;
		struct_field_offset(node->line, struct_addr.type.base_type.type_name, field_node->name, &field);
		if (field.location != IMMEDIATE) ERROR_RET(node->line);
		res->type.local = 0;
		res->type.base_type = field.type.base_type;
		pop_bc; // struct base address
		ld_hl_immed(field.immediate); // offset
		add_hl_bc; // base+offset
		push_hl;
	}
	else ERROR_RET(node->line);
}

void generate_statement(Node* statement);
void generate_block(Node* block);

void generate_call(Node* node)
{
	Node* p=node->parameters;
	byte param_count=0;
	while (p)
	{
		++param_count;
		Term res;
		if (p->data_type.type == ARRAY || p->data_type.sub_type == STRUCT)
		{
			get_node_address(p,&res);
			if (res.location!=STACK) ERROR_RET(node->line);
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
			case A:	 set_hl_a; break;
			default: ERROR_RET(node->line);
			}
			push_hl;
		}
		p=p->sibling;
	}
	add_unknown_address(node->name, write_offset+1);
	const byte cmd[] = {0xCD, 0x00, 0x00};
	WRITE(cmd);
	param_count<<=1; // word per param
	for(byte i=0;i<param_count;++i)
		inc_sp;
}

void generate_assignment(Node* node)
{
	Node* target_node = node->child;
	Term target_term;
	get_node_address(target_node,&target_term);
	word target_size = type_size(node->line, &target_term.type.base_type);
	Node* expr_node = target_node->sibling;
	
	Term source_term;
	calculate_expression(expr_node,&source_term);
	switch (source_term.location)
	{
	case IMMEDIATE:
		set_bc_immed(source_term.immediate);
		pop_hl;
		ld_mem_hl_c;
		if (target_size > 1)
		{
			inc_hl;
			ld_mem_hl_b;
		}
		break;
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
	default: ERROR_RET(node->line);
	}
}

byte invert_condition(byte b)
{
	if (b == 0x38) return 0x30;
	if (b == 0x30) return 0x38;
	if (b == 0x28) return 0x20;
	if (b == 0x20) return 0x28;
	ERROR_RET(0xFFFF);
	return 0;
}

word clear_condition_flag(byte b)
{
	if (b == 0x38) return 0xA7;		// JR C  ->  AND A
	if (b == 0x30) return 0x37;		// JR NC ->  SCF
	if (b == 0x28) return 0x01F6;	// JR Z -> OR 1
	if (b == 0x20) return 0xBF;		// JR NZ -> CP A
	ERROR_RET(0xFFFF);
	return 0;
}

byte generate_condition(Node* node)
{
	if (node->type == PIPE)
	{
		byte b = generate_condition(node->child);
		word success_end = sh_temp();
		add_unknown_address(success_end, write_offset+3);
		const byte left_cmd[] = { invert_condition(b), 0x03, 0xC3, 0x00, 0x00 };
		WRITE(left_cmd);
		b = generate_condition(node->child->sibling);
		byte right_cmd[] = { b, 0x02, 0x00, 0x00 };
		word fail = clear_condition_flag(b);
		right_cmd[2] = fail & 0xFF;
		right_cmd[3] = fail >> 8;
		WRITE(right_cmd);
		add_known_address(success_end, write_offset);
		return b;
	}
	else
	if (node->type == AMP)
	{
		byte b = generate_condition(node->child);
		word failure_end = sh_temp();
		add_unknown_address(failure_end, write_offset + 3);
		const byte left_cmd[] = { b, 0x03, 0xC3, 0x00, 0x00 };
		WRITE(left_cmd);
		b = generate_condition(node->child->sibling);
		byte right_cmd[] = { b, 0x02, 0x00, 0x00 };
		word fail = clear_condition_flag(b);
		right_cmd[2] = fail & 0xFF;
		right_cmd[3] = fail >> 8;
		add_known_address(failure_end, write_offset + 2);
		WRITE(right_cmd);
		return b;
	}
	else
	{
		if (node->type==LPAREN) return generate_condition(node->child);
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
					ERROR_RET(node->line);
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
					ERROR_RET(node->line);
				}
			}
			else ERROR_RET(node->line);
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
			default: ERROR_RET(node->line);
			}
		}
	}
	ERROR_RET(node->line);
	return 0;
}

void generate_cond_block(Node* node, byte loop)
{
	if (!node->parameters) ERROR_RET(node->line);
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

void generate_ifelse(Node* node)
{
	if (!node->parameters) ERROR_RET(node->line);
	byte jump = generate_condition(node->parameters);
	word end_of_true = sh_temp();
	add_unknown_address(end_of_true, write_offset + 3);
	const byte true_cmd[] = { jump, 0x03, 0xC3, 0x00, 0x00 };
	WRITE(true_cmd);
	generate_block(node->child); // True side of if-else
	word end_of_else = sh_temp();
	add_unknown_address(end_of_else, write_offset+1);
	const byte jump_to_end[] = { 0xC3, 0x00, 0x00 };
	WRITE(jump_to_end);
	add_known_address(end_of_true, write_offset);
	generate_block(node->child->sibling);
	add_known_address(end_of_else,write_offset);
}

void generate_statement(Node* statement)
{
	if (statement->type == ASSIGN) generate_assignment(statement);
	else if (statement->type == WHILE) generate_cond_block(statement,1);
	else if (statement->type == IF) generate_cond_block(statement,0);
	else if (statement->type == CALL) generate_call(statement);
	else if (statement->type == IFELSE) generate_ifelse(statement);
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

void generate_common_functions()
{
	Address address;
	address.name = sh_get("mult_bc_de");
	address.address = write_offset;
	vector_push(knowns, &address);
	const byte mult_code[] = {	0x21,0x00,0x00,0x78,0x06,0x10,0x29,0xCB,
								0x21,0x17,0x30,0x01,0x19,0x10,0xF7,0xC9 };
	WRITE(mult_code);
	address.name=sh_get("gpu_block");
	address.address = write_offset;
	vector_push(knowns, &address);
	const byte gpu_code[] = { 0xC1, 0xE1, 0xE5, 0xC5, 0x3E, 0x08, 0xCF, 0xC9 };
	WRITE(gpu_code);
}

void fill_unknowns()
{
	word i=0,n=vector_size(unknowns);
	for (; i < n; ++i)
	{
		Address* unk=(Address*)vector_access(unknowns,i);
		word addr=get_known_address(0xFFFF,unk->name);
		raw_write(unk->address,(byte*)&addr,2);
	}
}

void add_variable(Node* node)
{
	Variable var;
	var.type.local = 0;
	var.name = node->name;
	var.address = write_offset;
	var.size = var_size(node);
	var.type.base_type = node->data_type;
	vector_push(variables, &var);
	for(word i=0;i<var.size;++i)
		write_byte(0);
}

void add_struct(Node* node)
{
	Struct s;
	s.name= node->name;
	s.fields = vector_new(sizeof(Field));
	Node* child=node->child;
	Field field;
	while (child)
	{
		field.name = child->name;
		field.type = child->data_type;
		vector_push(s.fields, &field);
	}
	vector_push(structs, &s);
}

void add_function(Node* node)
{
	if (!node->child) // extern
		return;
	word globals_size = vector_size(variables);
	scan_parameters(node);
	word locals_size = scan_variables(node, 0, 1);
	generate_function(node, locals_size);
	vector_resize(variables, globals_size); // Remove local vars
}

byte generate_code(parse_node_func parse_node_, file_write_func fwf)
{
	parse_node = parse_node_;
	raw_write = fwf;

	byte header[] = { 0xC3, 0x00, 0x00 };
	WRITE(header);
	add_unknown_address(sh_get("main"), 1);
	generate_common_functions();
	while (1)
	{
		Node* node = parse_node();
		if (!node) break;
		switch (node->type)
		{
		case VAR:		add_variable(node); break;
		case STRUCT:	add_struct(node);	break;
		case FUN:		add_function(node);	break;
		default: ERROR_RET(node->line);
		}
		release_node(node);
	}
	//word globals_size = scan_variables(root, 0, 0);
	//word globals_count = vector_size(variables);
	//for (word i = 0; i < globals_size; ++i)		// Reserve room for globals
	//	write_byte(0);
	//Node* child = root->child;
	//while (child)
	//{
	//	if (child->type == FUN && child->child) // Not an extern
	//	{
	//		scan_parameters(child);
	//		word locals_size = scan_variables(child, 0, 1);
	//		generate_function(child, locals_size);
	//		vector_resize(variables, globals_count); // Remove local vars
	//	}
	//	child = child->sibling;
	//}
	fill_unknowns();
	return 1;
}

void gen_init()
{
	write_offset = 0;
	structs = vector_new(sizeof(Struct));
	variables = vector_new(sizeof(Variable));
	knowns = vector_new(sizeof(Address));
	unknowns = vector_new(sizeof(Address));
}

void gen_shut()
{
	vector_shut(unknowns);
	vector_shut(knowns);
	vector_shut(variables);
	word n=vector_size(structs);
	for (word i = 0; i < n; ++i)
	{
		Struct* s=vector_access(structs,i);
		vector_shut(s->fields);
	}
	vector_shut(structs);
}

